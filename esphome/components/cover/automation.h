#pragma once

namespace esphome::cover {

/// Whether a state trigger fires on this publish: once, on the publish that enters its state.
/// `seen` holds whether the trigger saw its state on the publish before. It is updated before this
/// returns, so call it on every publish, before running the trigger's actions.
inline bool state_entered(bool &seen, bool in_state) {
  const bool entered = in_state && !seen;
  seen = in_state;
  return entered;
}

}  // namespace esphome::cover
