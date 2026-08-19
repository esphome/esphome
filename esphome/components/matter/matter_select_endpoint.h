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
#endif  // matter supported variant
#endif  // USE_ESP_IDF
