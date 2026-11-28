#pragma once

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
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
