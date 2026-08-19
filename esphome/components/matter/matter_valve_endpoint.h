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
#ifdef USE_VALVE

#include <cstdint>
#include <atomic>

#include <app/clusters/valve-configuration-and-control-server/valve-configuration-and-control-delegate.h>
#include <app-common/zap-generated/cluster-enums.h>
#include <app/data-model/Nullable.h>
#include <lib/core/CHIPError.h>
#include <lib/core/DataModelTypes.h>

namespace esphome::valve {
class Valve;
}  // namespace esphome::valve

namespace esphome::matter {

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
  // in response to Open (0x00) / Close (0x01) commands. Names follow CHIP's
  // UpperCamelCase virtual method contract; NOLINT so clang-tidy's default
  // lower_case rule for ESPHome methods does not fire on the required
  // override signature.
  // NOLINTNEXTLINE(readability-identifier-naming)
  ::chip::app::DataModel::Nullable<::chip::Percent> HandleOpenValve(
      ::chip::app::DataModel::Nullable<::chip::Percent> level) override;
  // NOLINTNEXTLINE(readability-identifier-naming)
  CHIP_ERROR HandleCloseValve() override;
  // NOLINTNEXTLINE(readability-identifier-naming)
  void HandleRemainingDurationTick(uint32_t duration) override;

  uint16_t endpoint_id() const { return endpoint_id_; }
  ::esphome::valve::Valve *esphome_valve() const { return valve_; }
  bool applying_report() const { return this->applying_report_.load(std::memory_order_acquire); }

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
  std::atomic<bool> applying_report_{false};
};

}  // namespace esphome::matter

#endif  // USE_VALVE
#endif  // matter supported variant
#endif  // USE_ESP_IDF
