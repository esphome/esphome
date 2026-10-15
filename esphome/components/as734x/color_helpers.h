#pragma once
#include <cstdint>

namespace esphome::as734x {

// Smallest tristimulus sum that still carries a usable hue.
constexpr float MIN_TRISTIMULUS_SUM = 0.001f;

// Correlated colour temperature in kelvin from CIE 1931 tristimulus values, using McCamy's
// approximation. Returns NAN when the input carries no usable hue, or when the result falls
// outside the range the approximation holds over.
float tristimulus_to_cct(float x, float y, float z);

#ifdef USE_AS734X_RGB
// sRGB components for CIE 1931 tristimulus values. Pass chromaticity (x + y + z == 1) rather than
// absolute values, otherwise anything but dim light clamps to white.
void tristimulus_to_rgb(float x, float y, float z, uint8_t &r, uint8_t &g, uint8_t &b);
#endif

}  // namespace esphome::as734x
