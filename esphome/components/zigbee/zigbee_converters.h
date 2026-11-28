#pragma once

#include "esphome/core/defines.h"
#ifdef USE_ZIGBEE
#ifdef USE_LIGHT
#include "esphome/components/light/color_mode.h"

namespace esphome::zigbee {
// convertion between float 0.0 - 1.0 scale and Zigbee uint8_t scale 1 - 254 with 255 = NaN.
// theoretically 1 should map to the minimum brightness the light can still show, so should
// set this to 0+eps.
float level_to_brightness(uint8_t level);

uint8_t brightness_to_level(float brightness);

// Zigbee color temperature in mireds from 1 to 65279.
uint16_t convert_color_temp(float ct);

float convert_color_temp(uint16_t ct);

uint16_t get_color_temp(float ww, float cw, float min_mireds, float max_mireds);

// color temperature or XY
uint8_t get_color_mode(light::ColorMode mode);

// This follows Home Assistant's color_RGB_to_xy() conversion. ESPHome uses
// normalized RGB channel values instead of Home Assistant's 0-255 values, and
// Zigbee stores each resulting CIE xy component as a 16-bit value.
bool rgb_to_xy(float red, float green, float blue, uint16_t &x, uint16_t &y);

// This follows Home Assistant's color_xy_to_RGB() conversion. Zigbee provides
// CIE xy as 16-bit values while ESPHome expects normalized RGB channels.
bool xy_to_rgb(uint16_t zb_x, uint16_t zb_y, float &red, float &green, float &blue);

}  // namespace esphome::zigbee
#endif
#endif
