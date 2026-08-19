#pragma once

// Must be pulled in explicitly — none of the other headers this file uses
// bring in ESPHome's USE_* macro table, so without this the #ifdef guards
// below would strip the entire file at preprocess time.
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
#ifdef USE_SWITCH

#include <cstdint>
#include <atomic>

// Forward declaration keeps the switch header out of this .h. The esp-matter
// node handle is opaque here — the .cpp fetches it via esp_matter::node::get().
namespace esphome::switch_ {
class Switch;
}  // namespace esphome::switch_

namespace esphome::matter {

// Wraps one ESPHome switch as a Matter on_off_plug_in_unit endpoint.
//
// Lifecycle:
//   1. Constructed by MatterComponent for each non-internal Switch it finds
//      in App.get_switches() at setup() time.
//   2. setup(node) creates the Matter endpoint (auto-assigned id) and hooks
//      the ESPHome state callback so device→fabric updates flow through.
//   3. on_matter_write(bool) is called by the MatterComponent's global
//      attribute dispatcher when the fabric writes OnOff.OnOff — it calls
//      switch_->turn_on()/turn_off() as appropriate.
//   4. push_initial_state() is called by MatterComponent after
//      esp_matter::start() to sync the current switch state to any freshly
//      subscribed controllers.
class MatterSwitchEndpoint {
 public:
  explicit MatterSwitchEndpoint(switch_::Switch *sw);

  // Creates the Matter endpoint and wires the device→fabric callback.
  // Returns true on success. Sets endpoint_id_ as a side effect.
  // The node handle is looked up internally via esp_matter::node::get().
  bool setup();

  // Called by the global attribute update dispatcher in MatterComponent
  // when the fabric writes to OnOff.OnOff on this endpoint.
  void on_matter_write(bool state);

  // Called after esp_matter::start() to publish the switch's current state.
  void push_initial_state();

  uint16_t endpoint_id() const { return endpoint_id_; }
  switch_::Switch *esphome_switch() const { return switch_; }

 protected:
  // Pushes the given state to the Matter fabric via esp_matter::attribute::report.
  // Marked protected so ESP-IDF logging in the .cpp can call this consistently.
  void report_state_to_fabric_(bool state);

  switch_::Switch *switch_;
  uint16_t endpoint_id_{0};
  // Guards against callback re-entry in TWO directions:
  //
  // applying_matter_write_: set while we drive switch_->turn_on()/off() from
  // on_matter_write(). Suppresses the ESPHome state callback → report loop
  // because the fabric already advanced its own view when it issued the
  // command.
  //
  // applying_report_: set while we call esp_matter::attribute::update() from
  // report_state_to_fabric_(). update() fires the PRE_UPDATE callback
  // synchronously, which lands in MatterComponent::attribute_update_cb and
  // gets dispatched right back at on_matter_write() — that would call
  // switch_->turn_on() again and (potentially) loop. Skipping the dispatch
  // while this flag is set breaks the round-trip.
  bool applying_matter_write_{false};
  std::atomic<bool> applying_report_{false};

 public:
  bool applying_report() const { return this->applying_report_.load(std::memory_order_acquire); }
};

}  // namespace esphome::matter

#endif  // USE_SWITCH
#endif  // matter supported variant
#endif  // USE_ESP_IDF
