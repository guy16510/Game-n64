#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include "asteroid_pilot/input_state.hpp"

namespace asteroid_pilot {

constexpr float kFixedDt = 1.0F / 60.0F;
constexpr std::size_t kMaxAsteroids = 32;
constexpr std::size_t kMaxProjectiles = 24;

struct Vec3 {
    float x = 0.0F;
    float y = 0.0F;
    float z = 0.0F;
};

struct ShipState {
    float pitch = 0.0F;
    float roll = 0.0F;
    float yaw_rate = 0.0F;
    float speed = 7.0F;
};

struct AsteroidState {
    Vec3 position{};
    Vec3 velocity{};
    float radius = 1.0F;
    bool active = false;
};

struct ProjectileState {
    Vec3 position{};
    float speed = 18.0F;
    bool active = false;
};

struct GameState {
    ShipState ship{};
    std::array<AsteroidState, kMaxAsteroids> asteroids{};
    std::array<ProjectileState, kMaxProjectiles> projectiles{};
    std::uint64_t frame = 0;
    std::uint32_t score = 0;
    std::uint32_t rng_state = 1;
    bool previous_fire = false;
};

void reset_game(GameState& state, std::uint32_t seed = 0x0A57E201U);
void step_game(GameState& state, const InputState& input, float dt = kFixedDt);

std::size_t active_asteroid_count(const GameState& state);
std::size_t active_projectile_count(const GameState& state);

// Stable, quantized digest intended for deterministic replay tests. It is not
// a cryptographic hash and is deliberately independent of object addresses.
std::uint64_t state_digest(const GameState& state);

}  // namespace asteroid_pilot
