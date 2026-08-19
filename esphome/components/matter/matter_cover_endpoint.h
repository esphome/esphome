#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_COVER

#include <cstdint>
#include <atomic>

namespace esphome::cover {
class Cover;
}  // namespace esphome::cover

namespace esphome::matter {

// Wraps one ESPHome cover as a Matter window_covering endpoint
// (Lift + PositionAwareLift). Bidirectional.
//
// Position mapping:
//   ESPHome uses position 0.0 = fully closed, 1.0 = fully open (float).
//   Matter WindowCovering uses percent100ths 0 = fully open,
//   10000 = fully closed (uint16). Both directions invert.
//
// Fabric → device: PRE_UPDATE on TargetPositionLiftPercent100ths lands in
// MatterComponent::handle_cover_target_write, which routes to
// on_matter_target_write() below. That drives the ESPHome cover via
// make_call().set_position().perform() (or open/close command when the
// cover does not advertise supports_position).
//
// Device → fabric: cover state callback reads cover_->position, inverts,
// and writes CurrentPositionLiftPercent100ths through attribute::update.
// When the cover reaches idle, we also write TargetPositionLiftPercent100ths
// to match — otherwise the CHIP window-covering server keeps reporting the
// cover as still moving.
class MatterCoverEndpoint {
 public:
  explicit MatterCoverEndpoint(cover::Cover *cover);

  bool setup();

  // Called by MatterComponent's global dispatcher when the fabric writes
  // WindowCovering.TargetPositionLiftPercent100ths on this endpoint.
  // percent100ths: 0 (open) .. 10000 (closed).
  void on_matter_target_write(uint16_t percent100ths);

  void push_initial_state();

  uint16_t endpoint_id() const { return endpoint_id_; }
  cover::Cover *esphome_cover() const { return cover_; }
  bool applying_report() const { return this->applying_report_.load(std::memory_order_acquire); }

 protected:
  // Writes current position (and matching target when idle) to the fabric.
  void report_state_to_fabric_();

  cover::Cover *cover_;
  uint16_t endpoint_id_{0};
  // Suppresses the device→fabric report while we are the ones driving the
  // ESPHome cover from on_matter_target_write() — the fabric already knows
  // the target it just sent, and the intermediate current position echoes
  // through publish_state() would race the Target write.
  bool applying_matter_write_{false};
  // Suppresses the fabric→device dispatch while attribute::update() fires
  // PRE_UPDATE synchronously from our own report path.
  std::atomic<bool> applying_report_{false};
  // Cached traits.supports_position — checked once at setup(). Determines
  // whether on_matter_target_write drives set_position() or falls back to
  // open/close command based on a threshold.
  bool supports_position_{false};
};

}  // namespace esphome::matter

#endif  // USE_COVER
#endif  // USE_ESP_IDF
