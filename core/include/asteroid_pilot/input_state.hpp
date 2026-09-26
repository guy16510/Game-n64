#pragma once

namespace asteroid_pilot {

struct InputState {
    float pitch = 0.0F;   // normalized -1..1
    float roll = 0.0F;    // normalized -1..1
    float yaw_rate = 0.0F;
    bool fire = false;
    bool boost = false;
};

}  // namespace asteroid_pilot
