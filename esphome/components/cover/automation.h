#pragma once

#include <cstdint>

namespace esphome::cover {

/// Whether a state trigger fires on this publish: once, on the publish that enters its state.
/// Bit `mask` of `seen` holds whether the trigger saw its state on the publish before.
inline bool state_entered(uint8_t &seen, uint8_t mask, bool in_state) {
  if (!in_state) {
    seen &= ~mask;
    return false;
  }
  if (seen & mask)
    return false;
  seen |= mask;
  return true;
}

}  // namespace esphome::cover
