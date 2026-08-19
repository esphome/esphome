#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_BUTTON

#include <cstdint>

namespace esphome::button {
class Button;
}  // namespace esphome::button

namespace esphome::matter {

// Wraps one ESPHome button as a Matter generic_switch endpoint.
//
// Buttons don't have state — they only emit press events. The Matter Switch
// cluster mirrors this: for momentary switches (feature MS + MSR), it uses
// InitialPress and ShortRelease *events* rather than attribute writes.
//
// The endpoint advertises MS (0x2) + MSR (0x4) = feature_flags 0x6. The Switch
// cluster's create() honors config.feature_flags (unlike FanControl), so we
// don't need a post-create feature add — the create call itself pulls in the
// event definitions.
//
// Device → fabric only (buttons are output-only from ESPHome's perspective).
// On each ESPHome press:
//   1. attribute::update CurrentPosition = 1 — spec says position SHALL be 1
//      while a momentary switch is pressed
//   2. event::send_initial_press(1)
//   3. attribute::update CurrentPosition = 0 — released
//   4. event::send_short_release(1)
// All fires synchronously — ESPHome's button::press() is instantaneous, so we
// simulate the press/release cycle in one callback invocation.
class MatterButtonEndpoint {
 public:
  explicit MatterButtonEndpoint(::esphome::button::Button *button);

  bool setup();
  // Buttons have no persistent state to push on boot — this is a no-op kept
  // for symmetry with the other wrappers so MatterComponent can iterate all
  // endpoints uniformly.
  void push_initial_state() {}

  uint16_t endpoint_id() const { return endpoint_id_; }
  ::esphome::button::Button *esphome_button() const { return button_; }

 protected:
  void emit_press_cycle_();

  ::esphome::button::Button *button_;
  uint16_t endpoint_id_{0};
};

}  // namespace esphome::matter

#endif  // USE_BUTTON
#endif  // USE_ESP_IDF
