#pragma once

// Must be pulled in explicitly — none of the other headers this file uses
// bring in ESPHome's USE_* macro table, so without this the #ifdef guards
// below would strip the entire file at preprocess time.
#include "esphome/core/defines.h"

// USE_MATTER_VARIANT_SUPPORTED is set by matter's Python to_code() via
// cg.add_define() on the 5 esp-matter-supported ESP32 variants (ESP32,
// S3, C3, C6, H2). It is deliberately NOT declared in
// esphome/core/defines.h — that path is only exercised by clang-tidy and
// static-analysis tools, which do not have esp_matter.h available (the
// SDK is a third-party managed component fetched at build time). Keeping
// the symbol out of defines.h means matter files strip on lint (no
// missing-header errors) but compile normally on real builds where
// Python-side codegen has run. Runtime variant enforcement lives in the
// only_on_variant validator in matter/__init__.py.
#if defined(USE_ESP_IDF) && defined(USE_MATTER_VARIANT_SUPPORTED)
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
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
