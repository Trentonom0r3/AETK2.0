#pragma once

namespace aetk::effect::ui {

/**
 * @brief Defines the travel bounds used by joystick-style controls.
 *
 * @details `circle` preserves the classic thumbstick feel by constraining motion
 * to a unit circle. `square` preserves independent full-range X/Y travel so
 * users can reach edge and corner combinations without radial compression.
 */
enum class joystick_shape {
    circle,
    square
};

} // namespace aetk::effect::ui
