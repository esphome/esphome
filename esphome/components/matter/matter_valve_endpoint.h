#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_VALVE

#include <cstdint>

#include <app/clusters/valve-configuration-and-control-server/valve-configuration-and-control-delegate.h>
#include <app-common/zap-generated/cluster-enums.h>
#include <app/data-model/Nullable.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>

namespace esphome {
namespace valve {
class Valve;
}
}  // namespace esphome

namespace esphome {
namespace matter {

// Wraps one ESPHome valve as a Matter water_valve endpoint.
//
// Fabric → device (Open/Close commands) goes through the CHIP Delegate pattern
// — NOT weak-symbol overrides (like DoorLock) and NOT attribute writes (like
// WindowCovering). The cluster's TargetState/CurrentState attributes are
// read-only for the fabric; only the Open (0x00) and Close (0x01) commands
// are writable. CHIP's valve-configuration-and-control-server calls our
// delegate methods via `Delegate::HandleOpenValve` / `HandleCloseValve` after
// stamping TargetState=kOpen/kClosed and CurrentState=kTransitioning itself.
//
// Device → fabric: ESPHome valve's `add_on_state_callback` fires on any
// position/current_operation change; we translate to ValveStateEnum
// (kOpen/kClosed/kTransitioning) and push via esp_matter::attribute::update
// on CurrentState. When we reach an idle terminal state (kOpen or kClosed),
// we also null out TargetState so the fabric stops showing "in transit" —
// this mirrors what CHIP's UpdateCurrentState helper does internally.
class MatterValveEndpoint : public ::chip::app::Clusters::ValveConfigurationAndControl::Delegate {
 public:
  explicit MatterValveEndpoint(::esphome::valve::Valve *valve);

  bool setup();
  void push_initial_state();

  // Delegate overrides — called by CHIP's valve-configuration-and-control-server
  // in response to Open (0x00) / Close (0x01) commands.
  ::chip::app::DataModel::Nullable<::chip::Percent> HandleOpenValve(
      ::chip::app::DataModel::Nullable<::chip::Percent> level) override;
  CHIP_ERROR HandleCloseValve() override;
  void HandleRemainingDurationTick(uint32_t duration) override;

  uint16_t endpoint_id() const { return endpoint_id_; }
  ::esphome::valve::Valve *esphome_valve() const { return valve_; }
  bool applying_report() const { return applying_report_; }

 protected:
  void report_state_to_fabric_();
  // ESPHome position/current_operation → Matter ValveStateEnum raw uint8:
  //   OPENING/CLOSING → 2 (kTransitioning)
  //   IDLE + position<=0.01 → 0 (kClosed)
  //   IDLE + position>0.01  → 1 (kOpen)
  uint8_t esphome_to_matter_state_() const;

  ::esphome::valve::Valve *valve_;
  uint16_t endpoint_id_{0};
  // Kept for symmetry with the other bidirectional wrappers. Not currently
  // read anywhere (valve has no PRE_UPDATE dispatch loop today), but the
  // state-callback path sets it around attribute::update() so a future
  // dispatcher branch on CurrentState/TargetState can suppress re-entry.
  bool applying_report_{false};
};

}  // namespace matter
}  // namespace esphome

#endif  // USE_VALVE
#endif  // USE_ESP_IDF
