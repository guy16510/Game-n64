#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>

#include "asteroid_pilot/game_state.hpp"

namespace {

void require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << "TEST FAILURE: " << message << '\n';
        std::exit(1);
    }
}

void deterministic_replay_test() {
    using namespace asteroid_pilot;

    GameState a{};
    GameState b{};
    reset_game(a, 0x12345678U);
    reset_game(b, 0x12345678U);

    for (std::uint64_t frame = 0; frame < 720ULL; ++frame) {
        InputState input{};
        input.roll = static_cast<float>(static_cast<int>(frame % 100ULL) - 50) / 60.0F;
        input.pitch = static_cast<float>(static_cast<int>(frame % 80ULL) - 40) / 70.0F;
        input.fire = (frame % 37ULL) == 0ULL;
        input.boost = (frame % 160ULL) >= 120ULL;

        step_game(a, input);
        step_game(b, input);
    }

    require(a.frame == b.frame, "replay frame mismatch");
    require(a.score == b.score, "replay score mismatch");
    require(state_digest(a) == state_digest(b), "same replay must produce same digest");
}

void seed_changes_world_test() {
    using namespace asteroid_pilot;

    GameState a{};
    GameState b{};
    reset_game(a, 1U);
    reset_game(b, 2U);

    InputState input{};
    for (int frame = 0; frame < 120; ++frame) {
        step_game(a, input);
        step_game(b, input);
    }

    require(state_digest(a) != state_digest(b), "different seeds should produce different worlds");
}

void fire_is_edge_triggered_test() {
    using namespace asteroid_pilot;

    GameState state{};
    reset_game(state, 99U);

    InputState input{};
    input.fire = true;
    for (int frame = 0; frame < 10; ++frame) {
        step_game(state, input);
    }

    require(active_projectile_count(state) == 1U, "holding fire should create one edge-triggered shot");

    input.fire = false;
    step_game(state, input);
    input.fire = true;
    step_game(state, input);

    require(active_projectile_count(state) == 2U, "second press should create second shot");
}

void boost_changes_speed_test() {
    using namespace asteroid_pilot;

    GameState state{};
    reset_game(state, 44U);

    InputState input{};
    step_game(state, input);
    const float normal_speed = state.ship.speed;

    input.boost = true;
    step_game(state, input);
    require(state.ship.speed > normal_speed, "boost must increase ship speed");
}

void controls_are_bounded_test() {
    using namespace asteroid_pilot;

    GameState state{};
    reset_game(state, 55U);

    InputState input{};
    input.roll = 100.0F;
    input.pitch = -100.0F;

    for (int frame = 0; frame < 240; ++frame) {
        step_game(state, input);
    }

    require(std::fabs(state.ship.roll) <= 1.101F, "roll should remain bounded");
    require(std::fabs(state.ship.pitch) <= 0.751F, "pitch should remain bounded");
}

}  // namespace

int main() {
    deterministic_replay_test();
    seed_changes_world_test();
    fire_is_edge_triggered_test();
    boost_changes_speed_test();
    controls_are_bounded_test();

    std::cout << "All asteroid-pilot core tests passed.\n";
    return 0;
}
