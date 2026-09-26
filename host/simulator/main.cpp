#include <cstdint>
#include <iomanip>
#include <iostream>

#include "asteroid_pilot/game_state.hpp"

int main() {
    using namespace asteroid_pilot;

    GameState state{};
    reset_game(state, 0x0A57E201U);

    for (std::uint64_t frame = 0; frame < 900ULL; ++frame) {
        InputState input{};

        const int roll_phase = static_cast<int>(frame % 120ULL) - 60;
        const int pitch_phase = static_cast<int>(frame % 90ULL) - 45;

        input.roll = static_cast<float>(roll_phase) / 75.0F;
        input.pitch = static_cast<float>(pitch_phase) / 90.0F;
        input.fire = (frame % 45ULL) == 0ULL;
        input.boost = (frame % 200ULL) >= 150ULL;

        step_game(state, input);
    }

    std::cout << "{\n"
              << "  \"frames\": " << state.frame << ",\n"
              << "  \"score\": " << state.score << ",\n"
              << "  \"active_asteroids\": " << active_asteroid_count(state) << ",\n"
              << "  \"active_projectiles\": " << active_projectile_count(state) << ",\n"
              << "  \"digest\": \"0x" << std::hex << std::setw(16) << std::setfill('0')
              << state_digest(state) << "\"\n"
              << "}\n";

    return 0;
}
