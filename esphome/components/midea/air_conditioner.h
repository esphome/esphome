#pragma once

#ifdef USE_ARDUINO

// MideaUART
#include <Appliance/AirConditioner/AirConditioner.h>

#include "appliance_base.h"
#include "esphome/components/sensor/sensor.h"
#include <cmath>

namespace esphome {
namespace midea {
namespace ac {

using sensor::Sensor;
using climate::ClimateCall;
using climate::ClimatePreset;
using climate::ClimateTraits;
using climate::ClimateMode;
using climate::ClimateSwingMode;
using climate::ClimateFanMode;
using climate::ClimateModeMask;
using climate::ClimateSwingModeMask;
using climate::ClimatePresetMask;

class AirConditioner : public ApplianceBase<dudanov::midea::ac::AirConditioner>, public climate::Climate {
 public:
  void dump_config() override;
  void set_outdoor_temperature_sensor(Sensor *sensor) { this->outdoor_sensor_ = sensor; }
  void set_humidity_setpoint_sensor(Sensor *sensor) { this->humidity_sensor_ = sensor; }
  void set_power_sensor(Sensor *sensor) { this->power_sensor_ = sensor; }
  void on_status_change() override;

  /* ############### */
  /* ### ACTIONS ### */
  /* ############### */

  void do_follow_me(float temperature, bool use_fahrenheit, bool beeper = false);
  void do_display_toggle();
  void do_swing_step();
  void do_beeper_on() { this->set_beeper_feedback(true); }
  void do_beeper_off() { this->set_beeper_feedback(false); }
  void do_power_on() { this->base_.setPowerState(true); }
  void do_power_off() { this->base_.setPowerState(false); }
  void do_power_toggle() { this->base_.setPowerState(this->mode == ClimateMode::CLIMATE_MODE_OFF); }
  void set_supported_modes(ClimateModeMask modes) { this->supported_modes_ = modes; }
  void set_supported_swing_modes(ClimateSwingModeMask modes) { this->supported_swing_modes_ = modes; }
  void set_supported_presets(ClimatePresetMask presets) { this->supported_presets_ = presets; }
  void set_custom_presets(std::initializer_list<const char *> presets) { this->supported_custom_presets_ = presets; }
  void set_custom_fan_modes(std::initializer_list<const char *> modes) { this->supported_custom_fan_modes_ = modes; }
  void set_use_fahrenheit(bool state) { this->use_fahrenheit_ = state; }

 protected:
  void control(const ClimateCall &call) override;
  ClimateTraits traits() override;
  ClimateModeMask supported_modes_{};
  ClimateSwingModeMask supported_swing_modes_{};
  ClimatePresetMask supported_presets_{};
  std::vector<const char *> supported_custom_presets_{};
  std::vector<const char *> supported_custom_fan_modes_{};
  Sensor *outdoor_sensor_{nullptr};
  Sensor *humidity_sensor_{nullptr};
  Sensor *power_sensor_{nullptr};
  bool use_fahrenheit_{false};

  // AC's canonical Celsius values for each integer F (60-86).
  // Derived empirically by setting each F on the IR remote and reading
  // the Celsius value the AC reports back over UART.
  // Index 0 = 60F, Index 26 = 86F.
  static constexpr int F_MIN = 60;
  static constexpr int F_MAX = 86;
  static constexpr int F_COUNT = F_MAX - F_MIN + 1;
  static constexpr float AC_CELSIUS[F_COUNT] = {
    16.0f, 16.5f, 17.0f, 17.5f, 18.0f, 18.5f, 19.0f, 19.5f, 20.0f,  // 60-68
    20.5f, 21.0f, 21.5f, 22.0f, 23.0f, 23.5f, 24.0f, 24.5f, 25.0f,  // 69-77
    25.5f, 26.0f, 26.5f, 27.0f, 28.0f, 28.5f, 29.0f, 29.5f, 30.0f   // 78-86
  };

  /// INBOUND (HA -> AC): Convert HA's precise Celsius to the AC's canonical
  /// 0.5C value. HA sends (F-32)/1.8 which doesn't land on 0.5C steps.
  float ha_celsius_to_ac_celsius(float celsius) {
    if (!this->use_fahrenheit_)
      return celsius;
    int f = static_cast<int>(std::round(celsius * 1.8f + 32.0f));
    if (f < F_MIN) f = F_MIN;
    if (f > F_MAX) f = F_MAX;
    return AC_CELSIUS[f - F_MIN];
  }

  /// OUTBOUND (AC -> HA): Convert the AC's canonical 0.5C value to the
  /// precise Celsius that HA will display as a clean integer F.
  float ac_celsius_to_ha_celsius(float celsius) {
    if (!this->use_fahrenheit_)
      return celsius;
    // Find this value in the AC table
    for (int i = 0; i < F_COUNT; i++) {
      if (std::abs(AC_CELSIUS[i] - celsius) < 0.01f) {
        int f = F_MIN + i;
        return (f - 32.0f) / 1.8f;
      }
    }
    // Not in table, pass through unchanged
    return celsius;
  }
};

}  // namespace ac
}  // namespace midea
}  // namespace esphome

#endif  // USE_ARDUINO
