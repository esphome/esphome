#pragma once

#if (defined(USE_ARDUINO) && !defined(USE_RP2) && !defined(USE_LIBRETINY)) || defined(USE_ESP_IDF)

// MideaUART
#include <Appliance/AirConditioner/AirConditioner.h>

#include "appliance_base.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome::midea::ac {

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

// How the display toggle reaches the appliance; the AUTO rules are in do_display_toggle()
enum class DisplayControl : uint8_t {
  DISPLAY_CONTROL_UART,  // always the UART request
  DISPLAY_CONTROL_AUTO,  // UART when the appliance reports it or no transmitter is built in, else IR
  DISPLAY_CONTROL_IR,    // always the IR remote, needs a transmitter
};

class AirConditioner final : public ApplianceBase<dudanov::midea::ac::AirConditioner>, public climate::Climate {
  SUB_SENSOR(outdoor_temperature)
  SUB_SENSOR(humidity_setpoint)
  SUB_SENSOR(power)

 public:
  void dump_config() override;
  void set_display_control(DisplayControl control) { this->display_control_ = control; }
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

 protected:
  void control(const ClimateCall &call) override;
  ClimateTraits traits() override;
  ClimateModeMask supported_modes_{};
  ClimateSwingModeMask supported_swing_modes_{};
  ClimatePresetMask supported_presets_{};
  bool frost_protection_set_{false};
  DisplayControl display_control_{DisplayControl::DISPLAY_CONTROL_AUTO};
};

}  // namespace esphome::midea::ac

#endif  // USE_ARDUINO || USE_ESP_IDF
