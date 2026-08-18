#pragma once

#include "esphome/core/defines.h"

#ifdef USE_ESP_IDF
#ifdef USE_SELECT

#include <cstdint>
#include <string>
#include <vector>

#include <app-common/zap-generated/cluster-objects.h>

namespace esphome {
namespace select {
class Select;
}
}  // namespace esphome

namespace esphome {
namespace matter {

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
  bool applying_report() const { return applying_report_; }

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
  bool applying_report_{false};
};

}  // namespace matter
}  // namespace esphome

#endif  // USE_SELECT
#endif  // USE_ESP_IDF
