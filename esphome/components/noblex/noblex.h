#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace noblex {

// Temperature
const uint8_t NOBLEX_TEMP_MIN = 16;  // Celsius
const uint8_t NOBLEX_TEMP_MAX = 30;  // Celsius

class NoblexClimate : public climate_ir::ClimateIR {
 public:
  NoblexClimate()
      : climate_ir::ClimateIR(NOBLEX_TEMP_MIN, NOBLEX_TEMP_MAX, 1.0f, true, true,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  void setup() override {
    climate_ir::ClimateIR::setup();
    this->powered_on_assumed = this->mode != climate::CLIMATE_MODE_OFF;
  }

  // Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override {
    send_swing_cmd_ = call.get_swing_mode().has_value();
    // swing resets after unit powered off
    if (call.get_mode().has_value() && *call.get_mode() == climate::CLIMATE_MODE_OFF)
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    climate_ir::ClimateIR::control(call);
  }

  // used to track when to send the power toggle command.
  bool powered_on_assumed;

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer.
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool send_swing_cmd_{false};
  bool receiving_ = false;
};

}  // namespace noblex
}  // namespace esphome
