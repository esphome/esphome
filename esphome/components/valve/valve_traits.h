#pragma once

#include <cstdint>

namespace esphome::valve {

class ValveTraits {
 public:
  ValveTraits() = default;

  bool get_is_assumed_state() const { return this->flags_.is_assumed_state; }
  void set_is_assumed_state(bool is_assumed_state) { this->flags_.is_assumed_state = is_assumed_state; }
  bool get_supports_position() const { return this->flags_.supports_position; }
  void set_supports_position(bool supports_position) { this->flags_.supports_position = supports_position; }
  bool get_supports_toggle() const { return this->flags_.supports_toggle; }
  void set_supports_toggle(bool supports_toggle) { this->flags_.supports_toggle = supports_toggle; }
  bool get_supports_stop() const { return this->flags_.supports_stop; }
  void set_supports_stop(bool supports_stop) { this->flags_.supports_stop = supports_stop; }

 protected:
  // Bit-packed flags to save memory (1 byte instead of 4)
  struct ValveTraitFlags {
    uint8_t is_assumed_state : 1;
    uint8_t supports_position : 1;
    uint8_t supports_toggle : 1;
    uint8_t supports_stop : 1;
    uint8_t reserved : 4;
  } flags_{};
};

}  // namespace esphome::valve
