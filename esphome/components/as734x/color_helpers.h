#pragma once
#include <cstdint>

namespace esphome::as734x {

// Correlated colour temperature in kelvin from CIE 1931 tristimulus values, using McCamy's
// approximation. Returns 0 when the tristimulus values are too small to give a meaningful hue.
float tristimulus_to_cct(float x, float y, float z);

// sRGB components for CIE 1931 tristimulus values. Pass chromaticity (x + y + z == 1) rather than
// absolute values, otherwise anything but dim light clamps to white.
void tristimulus_to_rgb(float x, float y, float z, uint8_t &r, uint8_t &g, uint8_t &b);

}  // namespace esphome::as734x
