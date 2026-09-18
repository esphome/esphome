#pragma once

#include <cstdint>

namespace esphome::light {

/// Which byte of an addressable LED's data carries each colour.
///
/// Built from a configuration string such as "GRB" or "WRGB": every field holds the
/// position that colour occupies in the bytes the strip expects. `w` is NO_WHITE when
/// the strip has no separate white channel.
struct ChannelColors {
  /// Value of `w` for a strip that only has red, green and blue channels.
  static constexpr uint8_t NO_WHITE = 0xFF;

  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t w;

  constexpr bool has_white() const { return this->w != NO_WHITE; }

  /// Write the order back out as text, e.g. "GRBW".
  ///
  /// `buf` must have room for at least 5 characters. Returns `buf` so the result can be
  /// passed straight to a log call.
  const char *to_string(char *buf) const {
    unsigned fields = (1u << this->r) | (1u << this->g) | (1u << this->b) | (this->has_white() ? 1u << this->w : 0u);
    if (fields == 0b111 || fields == 0b1111) {
      buf[this->r] = 'R';
      buf[this->g] = 'G';
      buf[this->b] = 'B';
      if (this->has_white()) {
        buf[this->w] = 'W';
        buf[4] = '\0';
      } else {
        buf[3] = '\0';
      }
    } else {
      buf[0] = '?';
      buf[1] = '\0';
    }
    return buf;
  }
};

}  // namespace esphome::light
