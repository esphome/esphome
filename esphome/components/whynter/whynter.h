#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

#include <cinttypes>

namespace esphome {
namespace whynter {

// Temperature
const uint8_t TEMP_MIN_C = 16;  // Celsius
const uint8_t TEMP_MAX_C = 32;  // Celsius
const uint8_t TEMP_MIN_F = 61;  // Fahrenheit
const uint8_t TEMP_MAX_F = 89;  // Fahrenheit

class Whynter : public climate_ir::ClimateIR {
 public:
  Whynter()
      : climate_ir::ClimateIR(TEMP_MIN_C, TEMP_MAX_C, 1.0, true, true,
                              {climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH}, {}) {}

  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override { climate_ir::ClimateIR::control(call); }

  // Set use of Fahrenheit units
  void set_fahrenheit(bool value) {
    this->fahrenheit_ = value;
    this->temperature_step_ = 1.0f;
    this->minimum_temperature_ = esphome::fahrenheit_to_celsius(TEMP_MIN_F);
    this->maximum_temperature_ = esphome::fahrenheit_to_celsius(TEMP_MAX_F);
  }

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;

  void transmit_(uint32_t value);

  uint32_t header_high_ = 8000;
  uint32_t header_low_ = 4000;
  uint32_t bit_high_ = 600;
  uint32_t bit_one_low_ = 1600;
  uint32_t bit_zero_low_ = 550;

  bool fahrenheit_{false};

  climate::ClimateMode mode_before_{climate::CLIMATE_MODE_OFF};
};

}  // namespace whynter
}  // namespace esphome
