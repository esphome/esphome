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
#ifdef USE_LIGHT

#include <cstdint>
#include <atomic>

#include "esphome/components/light/light_state.h"

namespace esphome::matter {

// Wraps one ESPHome light as a Matter light endpoint. The concrete Matter
// device type is picked at setup() from the entity's LightTraits:
//   • RGB → extended_color_light        (OnOff + LevelControl + ColorControl XY+CT)
//   • COLOR_TEMPERATURE (no RGB) → color_temperature_light   (OnOff + LevelControl + ColorControl CT)
//   • BRIGHTNESS (no color) → dimmable_light                 (OnOff + LevelControl)
//   • else → on_off_light                                    (OnOff only)
//
// Bidirectional attribute mapping:
//   OnOff.OnOff                      (bool)          ↔ light.state
//   LevelControl.CurrentLevel        (uint8 0..254)  ↔ light.brightness (0..1 float, dimmable+)
//   ColorControl.ColorTemperatureMireds (uint16)     ↔ light.color_temperature (mireds float, CT+)
//
// Color XY / HueSaturation writes are NOT intercepted in this pass — the
// extended_color_light endpoint is still created (so RGB lights don't degrade
// to CT-only in the fabric UI) but color-change commands from the controller
// currently no-op on the device. Add XY/HS wiring in a follow-up iteration
// when a real RGB fixture is on the bench.
//
// Uses the LightRemoteValuesListener API rather than std::function callbacks
// — that's the officially-recommended low-overhead path per light_state.h:31.
class MatterLightEndpoint : public ::esphome::light::LightRemoteValuesListener {
 public:
  explicit MatterLightEndpoint(::esphome::light::LightState *light);

  bool setup();
  void push_initial_state();

  // Listener override — fires whenever the light's remote_values change
  // (state / brightness / color / CT). We translate into Matter attribute
  // updates on OnOff / LevelControl / ColorControl.
  void on_light_remote_values_update() override;

  // Dispatch entry points from MatterComponent's attribute_update_cb.
  void on_matter_on_off_write(bool state);
  void on_matter_level_write(uint8_t level);         // 0..254
  void on_matter_color_temp_write(uint16_t mireds);  // ColorTemperatureMireds

  uint16_t endpoint_id() const { return endpoint_id_; }
  ::esphome::light::LightState *esphome_light() const { return light_; }
  bool applying_report() const { return this->applying_report_.load(std::memory_order_acquire); }

  // Which Matter light device type we advertised at boot — needed by the
  // dispatcher so it can decide which write branches are legal for this
  // endpoint (e.g. dimmable_light has no ColorControl cluster).
  // Values follow the AGENTS.md rule for enum classes — prefix each with
  // the UPPER_SNAKE_CASE form of the enum name (CHIP's own `k`-prefix
  // convention is not used elsewhere in ESPHome).
  enum class DeviceKind : uint8_t {
    DEVICE_KIND_ON_OFF,
    DEVICE_KIND_DIMMABLE,
    DEVICE_KIND_COLOR_TEMPERATURE,
    DEVICE_KIND_EXTENDED_COLOR,
  };
  DeviceKind device_kind() const { return device_kind_; }

 protected:
  void report_state_to_fabric_();
  DeviceKind detect_device_kind_() const;

  ::esphome::light::LightState *light_;
  uint16_t endpoint_id_{0};
  DeviceKind device_kind_{DeviceKind::DEVICE_KIND_ON_OFF};
  // Suppress the ESPHome remote-values listener while we drive the light
  // from a matter write — otherwise the state change we just caused would
  // echo back to the fabric.
  bool applying_matter_write_{false};
  // Set while attribute::update() runs — PRE_UPDATE fires synchronously and
  // lands in the global dispatcher; the dispatcher checks this via
  // applying_report() to break the round-trip.
  std::atomic<bool> applying_report_{false};
  // Last non-zero level we observed on the ESPHome side. Serves as the
  // Matter LevelControl.CurrentLevel value while the light is off — CHIP's
  // Lighting-feature OnOff handler reads CurrentLevel during the On→Off
  // transition to save-as-OnLevel, and aborts with "ERR: Current Level is
  // null" if it finds null (even though spec §1.6.6.2 permits null while
  // off). Initialized to 254 (max) as a safe default before the first On.
  uint8_t last_known_level_{254};
};

}  // namespace esphome::matter

#endif  // USE_LIGHT
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
