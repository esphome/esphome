#pragma once

#include "esphome/core/defines.h"

// esp-matter 1.6.0 only supports these ESP32 variants. Strip the whole
// TU on any other target (P4, S2, C2, C5, C61, H4, H21, S31) so clang-tidy
// jobs for those variants — which grep this file in via USE_WIFI /
// USE_ETHERNET — don't try to compile against an esp_matter.h that upstream
// never ships for those chips. Runtime builds are already rejected by the
// only_on_variant config validator in matter/__init__.py; this guard is the
// static-analysis mirror of the same restriction.
#ifdef USE_ESP_IDF
#if defined(USE_ESP32_VARIANT_ESP32) || defined(USE_ESP32_VARIANT_ESP32S3) || defined(USE_ESP32_VARIANT_ESP32C3) || \
    defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32H2)
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
#endif  // matter supported variant
#endif  // USE_ESP_IDF
