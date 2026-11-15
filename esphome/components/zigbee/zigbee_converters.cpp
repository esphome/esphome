#include "zigbee_converters.h"
#ifdef USE_ZIGBEE
#ifdef USE_LIGHT

namespace esphome::zigbee {
// convertion between float 0.0 - 1.0 scale and Zigbee uint8_t scale 1 - 254 with 255 = NaN.
// theoretically 1 should map to the minimum brightness the light can still show, so should
// set this to 0+eps.
float level_to_brightness(uint8_t level) {
  if (level == 255)
    return std::numeric_limits<float>::quiet_NaN();
  if (level == 0)
    return std::numeric_limits<float>::min();
  return static_cast<float>(level - 1) / 253;
}

uint8_t brightness_to_level(float brightness) {
  if (std::isnan(brightness))
    return 255;
  return static_cast<uint8_t>(roundf(253.0f * brightness) + 1);  // 1 to 254
}

// Zigbee color temperature in mireds from 1 to 65279.
uint16_t convert_color_temp(float ct) {
  if (std::isnan(ct) || ct < 1 || ct > 65279)
    return 0;
  return static_cast<uint16_t>(roundf(ct));
}

float convert_color_temp(uint16_t ct) {
  if (ct == 0 || ct == 0xFFFF)
    return std::numeric_limits<float>::quiet_NaN();
  return static_cast<float>(ct);
}

uint16_t get_color_temp(float ww, float cw, float min_mireds, float max_mireds) {
  return convert_color_temp(ww / (ww + cw) * (min_mireds + max_mireds) + min_mireds);
}

// color temperature or XY
uint8_t get_color_mode(light::ColorMode mode) {
  if (!(mode & light::ColorCapability::RGB) &&
      ((mode & light::ColorCapability::COLOR_TEMPERATURE) || (mode & light::ColorCapability::COLD_WARM_WHITE))) {
    return 0x02;
  }
  // default to XY
  return 0x01;
}

// This follows Home Assistant's color_RGB_to_xy() conversion. ESPHome uses
// normalized RGB channel values instead of Home Assistant's 0-255 values, and
// Zigbee stores each resulting CIE xy component as a 16-bit value.
bool rgb_to_xy(float red, float green, float blue, uint16_t &x, uint16_t &y) {
  float r = std::clamp(red, 0.0f, 1.0f);
  float g = std::clamp(green, 0.0f, 1.0f);
  float b = std::clamp(blue, 0.0f, 1.0f);
  if (r + g + b == 0.0f) {
    x = 0;
    y = 0;
    return true;
  }

  auto gamma_decode = [](float channel) {
    return channel > 0.04045f ? std::pow((channel + 0.055f) / 1.055f, 2.4f) : channel / 12.92f;
  };
  r = gamma_decode(r);
  g = gamma_decode(g);
  b = gamma_decode(b);

  float cie_x = r * 0.664511f + g * 0.154324f + b * 0.162028f;
  float cie_y = r * 0.283881f + g * 0.668433f + b * 0.047685f;
  float cie_z = r * 0.000088f + g * 0.072310f + b * 0.986039f;
  float sum = cie_x + cie_y + cie_z;

  x = static_cast<uint16_t>(std::lroundf(std::clamp(cie_x / sum * 65536.0f, 0.0f, 65279.0f)));
  y = static_cast<uint16_t>(std::lroundf(std::clamp(cie_y / sum * 65536.0f, 0.0f, 65279.0f)));
  return true;
}

// This follows Home Assistant's color_xy_to_RGB() conversion. Zigbee provides
// CIE xy as 16-bit values while ESPHome expects normalized RGB channels.
bool xy_to_rgb(uint16_t zb_x, uint16_t zb_y, float &red, float &green, float &blue) {
  float x = zb_x / 65536.0f;
  float y = zb_y / 65536.0f;
  if (y == 0.0f)
    return false;

  float cie_x = x / y;
  float cie_z = (1.0f - x - y) / y;

  float r = cie_x * 1.656492f - 0.354851f - cie_z * 0.255038f;
  float g = -cie_x * 0.707196f + 1.655397f + cie_z * 0.036152f;
  float b = cie_x * 0.051713f - 0.121364f + cie_z * 1.011530f;

  auto gamma_encode = [](float channel) {
    return channel <= 0.0031308f ? 12.92f * channel : 1.055f * std::pow(channel, 1.0f / 2.4f) - 0.055f;
  };
  r = std::max(0.0f, gamma_encode(r));
  g = std::max(0.0f, gamma_encode(g));
  b = std::max(0.0f, gamma_encode(b));

  float maximum = std::max({r, g, b});
  if (maximum > 1.0f) {
    r /= maximum;
    g /= maximum;
    b /= maximum;
  }

  red = r;
  green = g;
  blue = b;
  return true;
}

}  // namespace esphome::zigbee
#endif
#endif
