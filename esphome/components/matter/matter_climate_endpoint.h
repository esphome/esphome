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
#ifdef USE_CLIMATE

#include <cstdint>
#include <atomic>

namespace esphome::climate {
class Climate;
class ClimateCall;
}  // namespace esphome::climate

namespace esphome::matter {

// Wraps one ESPHome climate as a Matter thermostat endpoint (Thermostat cluster).
//
// Feature-flag selection is driven by the ESPHome ClimateTraits at setup:
//   supports_mode(HEAT)         → Heating feature
//   supports_mode(COOL)         → Cooling feature
//   supports_mode(HEAT_COOL)    → AutoMode feature (mandates Heating+Cooling)
// A climate that exposes neither HEAT nor COOL is skipped with a warning:
// esp-matter would VALIDATE_FEATURES_AT_LEAST_ONE on Thermostat and abort
// boot, so we cannot register a "no-op" thermostat endpoint even for OFF-only.
//
// Attribute mapping (fabric ↔ device):
//   LocalTemperature (0x0000, nullable int16)   ← climate.current_temperature
//   OccupiedHeatingSetpoint (0x0012, int16)     ↔ climate.target_temperature (heat)
//   OccupiedCoolingSetpoint (0x0011, int16)     ↔ climate.target_temperature (cool)
//   SystemMode (0x001C, enum8)                  ↔ climate.mode (Off/Auto/Cool/Heat)
//   ControlSequenceOfOperation (0x001B, enum8)  ← derived once from supported modes
//   Min/MaxHeatSetpointLimit + Cool variants    ← ClimateTraits visual min/max
//
// Temperature encoding: Matter uses int16 hundredths of a °C (2500 == 25.00°C),
// ESPHome uses float degrees C — trivial *100 / /100 conversion, no unit lookup.
// A ClimateTraits::get_temperature_unit()==FAHRENHEIT device still stores its
// state as Celsius internally in ESPHome, so no Fahrenheit path needed.
//
// Fabric → device: attribute_update_cb dispatches SystemMode / setpoint writes
// via MatterComponent::handle_climate_*_write, which routes here; we translate
// to a make_call().set_mode()/set_target_temperature()/perform().
//
// Device → fabric: climate state callback fires with no args (climate mirrors
// the pattern used by fan/switch) — re-read state and update LocalTemperature,
// setpoints and SystemMode via attribute::update().
//
// Loop guards mirror MatterFanEndpoint: applying_matter_write_ suppresses the
// device→fabric echo while we drive make_call(); applying_report_ suppresses
// the fabric→device dispatch when attribute::update() re-enters via PRE_UPDATE.
class MatterClimateEndpoint {
 public:
  explicit MatterClimateEndpoint(climate::Climate *climate);

  bool setup();

  // Called by the dispatcher when the fabric writes SystemMode (uint8 enum).
  void on_matter_system_mode_write(uint8_t system_mode);
  // Called by the dispatcher when the fabric writes OccupiedHeatingSetpoint or
  // OccupiedCoolingSetpoint (int16 hundredths of °C).
  void on_matter_heating_setpoint_write(int16_t hundredths);
  void on_matter_cooling_setpoint_write(int16_t hundredths);

  void push_initial_state();

  uint16_t endpoint_id() const { return endpoint_id_; }
  climate::Climate *esphome_climate() const { return climate_; }
  bool applying_report() const { return this->applying_report_.load(std::memory_order_acquire); }

 protected:
  void report_state_to_fabric_();

  climate::Climate *climate_;
  uint16_t endpoint_id_{0};

  // Feature flags decided at setup(), remembered so report_state_to_fabric_
  // knows which setpoint attributes it is allowed to touch (writing
  // OccupiedCoolingSetpoint on a heat-only endpoint returns UNSUPPORTED_WRITE).
  bool supports_heating_{false};
  bool supports_cooling_{false};
  bool supports_auto_{false};
  // True when the ESPHome climate reports CLIMATE_SUPPORTS_TWO_POINT_TARGET_
  // TEMPERATURE — i.e. it exposes target_temperature_low / _high. Without
  // this the "heat = target-0.5, cool = target+0.5" spread hack we used to
  // apply to satisfy Auto's MinSetpointDeadBand drifts the ESPHome target
  // downward on every fabric round-trip, so AutoMode is disabled and each
  // setpoint is published as-is (spec §4.3.7.5 makes the deadband check
  // conditional on AutoMode).
  bool supports_two_point_target_{false};

  // Probed at setup() by walking the created cluster's attribute list. If a
  // future esp-matter version stops creating the setpoint attributes for a
  // given feature (or we accidentally set a feature flag without a matching
  // config), report_state_to_fabric_ can skip the missing ones instead of
  // logging a warning every publish.
  bool has_heating_setpoint_attr_{false};
  bool has_cooling_setpoint_attr_{false};

  bool applying_matter_write_{false};
  std::atomic<bool> applying_report_{false};
};

}  // namespace esphome::matter

#endif  // USE_CLIMATE
#endif  // USE_ESP_IDF && USE_MATTER_VARIANT_SUPPORTED
