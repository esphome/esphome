#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

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

  constexpr ChannelColors() : r(0), g(0), b(0), w(NO_WHITE) {}
  constexpr ChannelColors(uint8_t r_offset, uint8_t g_offset, uint8_t b_offset)
      : r(r_offset), g(g_offset), b(b_offset), w(NO_WHITE) {}
  constexpr ChannelColors(uint8_t r_offset, uint8_t g_offset, uint8_t b_offset, uint8_t w_offset)
      : r(r_offset), g(g_offset), b(b_offset), w(w_offset) {}

  union {
    struct {
      uint8_t r;
      uint8_t g;
      uint8_t b;
      uint8_t w;
    };
    uint8_t raw[4];
  };

  inline bool has_white() const { return (this->w & 0x80) == 0; }

  inline size_t bytes_per_led() const { return this->w == NO_WHITE ? 3 : 4; }

  Color get(const uint8_t *color_data, int32_t index) const {
    if (this->w == NO_WHITE)
      return Color(color_data[index * 3 + this->r], color_data[index * 3 + this->g], color_data[index * 3 + this->b]);
    return Color(color_data[index * 4 + this->r], color_data[index * 4 + this->g], color_data[index * 4 + this->b],
                 this->has_white() ? color_data[index * 4 + this->w] : 0);
  }

  uint8_t get_channel(const uint8_t *color_data, int32_t index, uint8_t channel) const {
    const uint8_t offset = this->raw[channel];
    return (offset & 0x80) == 0 ? color_data[index * this->bytes_per_led() + offset] : 0;
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

  void set_channel(uint8_t *color_data, int32_t index, uint8_t channel, uint8_t value) const {
    const uint8_t offset = this->raw[channel];
    if ((offset & 0x80) == 0)
      color_data[index * this->bytes_per_led() + offset] = value;
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

static_assert(offsetof(ChannelColors, raw) == COLOR_CHANNEL_R);
static_assert(offsetof(ChannelColors, r) == COLOR_CHANNEL_R);
static_assert(offsetof(ChannelColors, g) == COLOR_CHANNEL_G);
static_assert(offsetof(ChannelColors, b) == COLOR_CHANNEL_B);
static_assert(offsetof(ChannelColors, w) == COLOR_CHANNEL_W);

}  // namespace esphome::light
