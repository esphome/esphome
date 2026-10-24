#pragma once

#include <cstdint>

#include "light_traits.h"
#include "esphome/core/color.h"

namespace esphome::light {

/// Which byte of an addressable LED's data carries each colour.
///
/// Built from a configuration string such as "GRB" or "WRGB": every field holds the
/// position that colour occupies in the bytes the strip expects. `w` is NO_WHITE when
/// the strip has no separate white channel.
struct ChannelColors {
  /// Value of `w` for an RGB LED strip that uses 3 bytes per led.
  static constexpr uint8_t NO_WHITE = 0xFF;
  /// Value of `w` for an RGB LED strip that uses 4 bytes per led where the 4th byte is reserved.
  static constexpr uint8_t RESERVED_WHITE = 0xFE;

  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;

  inline bool has_white() const { return (this->w & 0x80) == 0; }

  inline size_t bytes_per_led() const { return this->w == NO_WHITE ? 3 : 4; }

  Color get(const uint8_t *color_data, int32_t index) const {
    if (this->w == NO_WHITE)
      return Color(color_data[index * 3 + this->r], color_data[index * 3 + this->g], color_data[index * 3 + this->b]);
    return Color(color_data[index * 4 + this->r], color_data[index * 4 + this->g], color_data[index * 4 + this->b],
                 this->has_white() ? color_data[index * 4 + this->w] : 0);
  }
  uint8_t get_red(const uint8_t *color_data, int32_t index) const {
    return color_data[index * this->bytes_per_led() + this->r];
  }
  uint8_t get_green(const uint8_t *color_data, int32_t index) const {
    return color_data[index * this->bytes_per_led() + this->g];
  }
  uint8_t get_blue(const uint8_t *color_data, int32_t index) const {
    return color_data[index * this->bytes_per_led() + this->b];
  }
  uint8_t get_white(const uint8_t *color_data, int32_t index) const {
    return this->has_white() ? color_data[index * 4 + this->w] : 0;
  }

  void set(uint8_t *color_data, int32_t index, Color color) const {
    if (this->w == NO_WHITE) {
      color_data[index * 3 + this->r] = color.red;
      color_data[index * 3 + this->g] = color.green;
      color_data[index * 3 + this->b] = color.blue;
    } else {
      color_data[index * 4 + this->r] = color.red;
      color_data[index * 4 + this->g] = color.green;
      color_data[index * 4 + this->b] = color.blue;
      if (this->has_white())
        color_data[index * 4 + this->w] = color.white;
    }
  }
  void set_red(uint8_t *color_data, int32_t index, uint8_t red) const {
    color_data[index * this->bytes_per_led() + this->r] = red;
  }
  void set_green(uint8_t *color_data, int32_t index, uint8_t green) const {
    color_data[index * this->bytes_per_led() + this->g] = green;
  }
  void set_blue(uint8_t *color_data, int32_t index, uint8_t blue) const {
    color_data[index * this->bytes_per_led() + this->b] = blue;
  }
  void set_white(uint8_t *color_data, int32_t index, uint8_t white) const {
    if (this->has_white())
      color_data[index * 4 + this->w] = white;
  }

  /// Write the order back out as text, e.g. "GRBW".
  ///
  /// `buf` must have room for at least 5 characters. Returns `buf` so the result can be
  /// passed straight to a log call.
  const char *to_string(char *buf) const {
    memset(buf, 'X', 4);
    buf[this->r] = 'R';
    buf[this->g] = 'G';
    buf[this->b] = 'B';
    if (this->has_white()) {
      buf[this->w] = 'W';
    }
    buf[this->bytes_per_led()] = '\0';
    return buf;
  }

  /// Colour modes of a strip with this channel order.
  LightTraits traits() const {
    LightTraits traits;
    if (this->has_white()) {
      traits.set_supported_color_modes({ColorMode::RGB_WHITE, ColorMode::WHITE});
    } else {
      traits.set_supported_color_modes({ColorMode::RGB});
    }
    return traits;
  }
};

}  // namespace esphome::light
