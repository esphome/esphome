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
#ifdef USE_BUTTON

#include <cstdint>

#include "esphome/core/component.h"
#include "esphome/components/button/button.h"

namespace esphome::matter {

class MatterComponent;

// A `button: platform: matter` entity. On press it triggers a Matter
// management action against the local device — OpenCommissioningWindow
// or FactoryReset. Designed to grow (RevokeCommissioning, etc.) without
// changing the yaml surface.
//
// This is deliberately NOT the same thing as MatterButtonEndpoint (which
// wraps a plain ESPHome button as a Matter GenericSwitch endpoint). The
// scanner in MatterComponent::scan_and_register_buttons_ dynamic_casts
// each Button* against MatterActionButton and skips it so the Matter
// fabric never sees an endpoint whose sole purpose is to manage the
// fabric itself — that would be circular and expose a "click me to open
// pairing" endpoint over the very fabric it manages.
class MatterActionButton : public ::esphome::button::Button, public Component {
 public:
  // Values follow the AGENTS.md rule for enum classes — prefix each with
  // the UPPER_SNAKE_CASE form of the enum name.
  enum class Action : uint8_t {
    ACTION_OPEN_COMMISSIONING_WINDOW = 0,
    // Wipes the Matter fabric (chip-config NVS + fabric table) and reboots.
    // Delegates to esp_matter::factory_reset, which schedules the wipe on the
    // CHIP task and calls esp_restart after — the ESPHome loop never returns.
    ACTION_FACTORY_RESET = 1,
  };

  MatterActionButton();
  ~MatterActionButton();

  void set_matter_component(MatterComponent *matter) { this->matter_ = matter; }
  void set_action(Action action) { this->action_ = action; }
  // Only meaningful for OPEN_COMMISSIONING_WINDOW; ignored otherwise.
  // Clamped to [180, 900] by the receiver per Matter spec §11.19.9.1.
  void set_timeout_seconds(uint32_t seconds) { this->timeout_seconds_ = seconds; }

  // Non-RTTI type check — the CHIP/esp-matter build compiles with
  // -fno-rtti (`dynamic_cast` and `typeid` are unavailable). Each
  // MatterActionButton registers its own address in a function-local
  // static set from its constructor; the scanner queries membership
  // via this helper to decide whether to skip a Button pointer.
  static bool is_instance(const ::esphome::button::Button *btn);

  void dump_config() override;

 protected:
  void press_action() override;

  MatterComponent *matter_{nullptr};
  Action action_{Action::ACTION_OPEN_COMMISSIONING_WINDOW};
  uint32_t timeout_seconds_{300};
};

}  // namespace esphome::matter

#endif  // USE_BUTTON
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
