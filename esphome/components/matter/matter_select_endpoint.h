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
#ifdef USE_SELECT

#include <cstdint>
#include <atomic>
#include <string>
#include <vector>

#include <app-common/zap-generated/cluster-objects.h>

namespace esphome::select {
class Select;
}  // namespace esphome::select

namespace esphome::matter {

// Wraps one ESPHome select as a Matter mode_select endpoint.
//
// SupportedModes population is handled through the CHIP `SupportedModesManager`
// singleton — one manager for the entire process, dispatching per-endpoint via
// endpoint id. The manager lives in matter_select_endpoint.cpp as a Meyers
// singleton and is registered via `ModeSelect::setSupportedModesManager` on
// the first endpoint setup. Each MatterSelectEndpoint owns its own labels +
// mode_options list and registers itself into the manager's endpoint map.
//
// Label storage: ModeOptionStruct::Type holds a non-owning CharSpan; we keep
// the original strings alive in `option_labels_` for the lifetime of the
// endpoint. The vector is reserved once at setup (no later push_back) so
// string data pointers stay valid.
//
// Fabric → device (ChangeToMode command): CHIP's mode-select-server validates
// via the SupportedModesManager, then writes CurrentMode via
// `Attributes::CurrentMode::Set()` — which fires PRE_UPDATE on our node-level
// attribute callback in MatterComponent. The dispatcher routes to
// `on_matter_current_mode_write`.
//
// Device → fabric: select `add_on_state_callback` fires with the new index;
// we push it via esp_matter::attribute::update on CurrentMode. Guards prevent
// the echo loops in the same shape as the switch/cover/fan wrappers.
class MatterSelectEndpoint {
 public:
  explicit MatterSelectEndpoint(::esphome::select::Select *sel);

  bool setup();
  void push_initial_state();

  // Called by the MatterComponent dispatcher when the fabric writes
  // ModeSelect.CurrentMode on this endpoint (result of a ChangeToMode command).
  void on_matter_current_mode_write(uint8_t mode);

  uint16_t endpoint_id() const { return endpoint_id_; }
  ::esphome::select::Select *esphome_select() const { return select_; }
  bool applying_report() const { return this->applying_report_.load(std::memory_order_acquire); }

  // Exposed for the shared SupportedModesManager singleton.
  const std::vector<::chip::app::Clusters::ModeSelect::Structs::ModeOptionStruct::Type> &mode_options() const {
    return mode_options_;
  }

 protected:
  void report_state_to_fabric_(uint8_t mode);

  ::esphome::select::Select *select_;
  uint16_t endpoint_id_{0};
  // Owns the label strings CharSpans point into. Populated once during
  // setup() with reserve() sized to the entity's option count — MUST NOT
  // resize after that or all CharSpan pointers dangle.
  std::vector<std::string> option_labels_;
  std::vector<::chip::app::Clusters::ModeSelect::Structs::ModeOptionStruct::Type> mode_options_;
  bool applying_matter_write_{false};
  std::atomic<bool> applying_report_{false};
};

}  // namespace esphome::matter

#endif  // USE_SELECT
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
