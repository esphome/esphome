#pragma once

#include <cstdint>

#include "esp_color_view.h"
#include "light_traits.h"

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

  bool has_white() const { return this->w != NO_WHITE; }

  uint8_t bytes_per_led() const { return this->has_white() ? 4 : 3; }

  /// Write the order back out as text, e.g. "GRBW".
  ///
  /// `buf` must have room for at least 5 characters. Returns `buf` so the result can be
  /// passed straight to a log call.
  const char *to_string(char *buf) const {
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
  LightTraits traits() const;

  /// View of LED `index` in `buf`; a scratch pixel without effect data while either is unallocated.
  ESPColorView view(uint8_t *buf, uint8_t *effect_data, int32_t index, const ESPColorCorrection *correction) const {
    // After a failed setup() effects and update_state() still write pixels
    static uint8_t scratch[4];
    const bool ready = buf != nullptr && effect_data != nullptr;
    uint8_t *led = ready ? buf + index * this->bytes_per_led() : scratch;
    return {led + this->r,
            led + this->g,
            led + this->b,
            this->has_white() ? led + this->w : nullptr,
            ready ? effect_data + index : nullptr,
            correction};
  }
};

}  // namespace esphome::light
