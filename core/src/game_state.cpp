#include "asteroid_pilot/game_state.hpp"

#include <cmath>
#include <cstdint>

namespace asteroid_pilot {
namespace {

float clamp_unit(float value) {
    if (value < -1.0F) {
        return -1.0F;
    }
    if (value > 1.0F) {
        return 1.0F;
    }
    return value;
}

std::uint32_t next_random(std::uint32_t& state) {
    // xorshift32. State must never be zero.
    std::uint32_t x = state;
    x ^= x << 13U;
    x ^= x >> 17U;
    x ^= x << 5U;
    state = x;
    return x;
}

float random_01(std::uint32_t& state) {
    constexpr float kDenominator = 16777215.0F;
    const std::uint32_t value = next_random(state) & 0x00FFFFFFU;
    return static_cast<float>(value) / kDenominator;
}

float random_bipolar(std::uint32_t& state) {
    return (random_01(state) * 2.0F) - 1.0F;
}

AsteroidState* first_inactive_asteroid(GameState& state) {
    for (auto& asteroid : state.asteroids) {
        if (!asteroid.active) {
            return &asteroid;
        }
    }
    return nullptr;
}

ProjectileState* first_inactive_projectile(GameState& state) {
    for (auto& projectile : state.projectiles) {
        if (!projectile.active) {
            return &projectile;
        }
    }
    return nullptr;
}

void spawn_asteroid(GameState& state) {
    AsteroidState* asteroid = first_inactive_asteroid(state);
    if (asteroid == nullptr) {
        return;
    }

    const float rx = random_bipolar(state.rng_state);
    const float ry = random_bipolar(state.rng_state);
    const float rz = random_01(state.rng_state);
    const float rr = random_01(state.rng_state);

    asteroid->position = Vec3{rx * 4.5F, ry * 2.4F, 28.0F + (rz * 18.0F)};
    asteroid->velocity = Vec3{-rx * 0.15F, -ry * 0.08F, -(4.5F + (rz * 2.5F))};
    asteroid->radius = 0.65F + (rr * 0.75F);
    asteroid->active = true;
}

void fire_projectile(GameState& state) {
    ProjectileState* projectile = first_inactive_projectile(state);
    if (projectile == nullptr) {
        return;
    }

    projectile->position = Vec3{state.ship.roll * 0.9F, -state.ship.pitch * 0.6F, 1.0F};
    projectile->speed = 18.0F;
    projectile->active = true;
}

float distance_squared(const Vec3& a, const Vec3& b) {
    const float dx = a.x - b.x;
    const float dy = a.y - b.y;
    const float dz = a.z - b.z;
    return (dx * dx) + (dy * dy) + (dz * dz);
}

std::int32_t quantize(float value) {
    return static_cast<std::int32_t>(value * 1000.0F);
}

void hash_u8(std::uint64_t& hash, std::uint8_t value) {
    constexpr std::uint64_t kPrime = 1099511628211ULL;
    hash ^= static_cast<std::uint64_t>(value);
    hash *= kPrime;
}

void hash_u32(std::uint64_t& hash, std::uint32_t value) {
    for (unsigned shift = 0U; shift < 32U; shift += 8U) {
        hash_u8(hash, static_cast<std::uint8_t>((value >> shift) & 0xFFU));
    }
}

void hash_u64(std::uint64_t& hash, std::uint64_t value) {
    for (unsigned shift = 0U; shift < 64U; shift += 8U) {
        hash_u8(hash, static_cast<std::uint8_t>((value >> shift) & 0xFFULL));
    }
}

void hash_float(std::uint64_t& hash, float value) {
    hash_u32(hash, static_cast<std::uint32_t>(quantize(value)));
}

void hash_vec3(std::uint64_t& hash, const Vec3& value) {
    hash_float(hash, value.x);
    hash_float(hash, value.y);
    hash_float(hash, value.z);
}

}  // namespace

void reset_game(GameState& state, std::uint32_t seed) {
    state = GameState{};
    state.rng_state = seed == 0U ? 1U : seed;
    state.ship.speed = 7.0F;
}

void step_game(GameState& state, const InputState& input, float dt) {
    if (dt <= 0.0F) {
        return;
    }

    const float clamped_roll = clamp_unit(input.roll);
    const float clamped_pitch = clamp_unit(input.pitch);
    const float response = (dt * 6.0F) > 1.0F ? 1.0F : (dt * 6.0F);

    const float target_roll = clamped_roll * 1.10F;
    const float target_pitch = clamped_pitch * 0.75F;
    state.ship.roll += (target_roll - state.ship.roll) * response;
    state.ship.pitch += (target_pitch - state.ship.pitch) * response;
    state.ship.yaw_rate = clamp_unit(input.yaw_rate);
    state.ship.speed = input.boost ? 12.0F : 7.0F;

    if ((state.frame % 30ULL) == 0ULL) {
        spawn_asteroid(state);
    }

    if (input.fire && !state.previous_fire) {
        fire_projectile(state);
    }
    state.previous_fire = input.fire;

    for (auto& projectile : state.projectiles) {
        if (!projectile.active) {
            continue;
        }
        projectile.position.z += projectile.speed * dt;
        if (projectile.position.z > 60.0F) {
            projectile.active = false;
        }
    }

    const float relative_speed_scale = state.ship.speed / 7.0F;
    for (auto& asteroid : state.asteroids) {
        if (!asteroid.active) {
            continue;
        }
        asteroid.position.x += asteroid.velocity.x * dt;
        asteroid.position.y += asteroid.velocity.y * dt;
        asteroid.position.z += asteroid.velocity.z * dt * relative_speed_scale;
        if (asteroid.position.z < -3.0F) {
            asteroid.active = false;
        }
    }

    for (auto& projectile : state.projectiles) {
        if (!projectile.active) {
            continue;
        }

        for (auto& asteroid : state.asteroids) {
            if (!asteroid.active) {
                continue;
            }

            const float collision_radius = asteroid.radius + 0.18F;
            if (distance_squared(projectile.position, asteroid.position) <=
                (collision_radius * collision_radius)) {
                projectile.active = false;
                asteroid.active = false;
                state.score += 100U;
                break;
            }
        }
    }

    ++state.frame;
}

std::size_t active_asteroid_count(const GameState& state) {
    std::size_t count = 0U;
    for (const auto& asteroid : state.asteroids) {
        if (asteroid.active) {
            ++count;
        }
    }
    return count;
}

std::size_t active_projectile_count(const GameState& state) {
    std::size_t count = 0U;
    for (const auto& projectile : state.projectiles) {
        if (projectile.active) {
            ++count;
        }
    }
    return count;
}

std::uint64_t state_digest(const GameState& state) {
    std::uint64_t hash = 1469598103934665603ULL;

    hash_float(hash, state.ship.pitch);
    hash_float(hash, state.ship.roll);
    hash_float(hash, state.ship.yaw_rate);
    hash_float(hash, state.ship.speed);
    hash_u64(hash, state.frame);
    hash_u32(hash, state.score);
    hash_u32(hash, state.rng_state);
    hash_u8(hash, state.previous_fire ? 1U : 0U);

    for (const auto& asteroid : state.asteroids) {
        hash_u8(hash, asteroid.active ? 1U : 0U);
        hash_vec3(hash, asteroid.position);
        hash_vec3(hash, asteroid.velocity);
        hash_float(hash, asteroid.radius);
    }

    for (const auto& projectile : state.projectiles) {
        hash_u8(hash, projectile.active ? 1U : 0U);
        hash_vec3(hash, projectile.position);
        hash_float(hash, projectile.speed);
    }

    return hash;
}

}  // namespace asteroid_pilot
