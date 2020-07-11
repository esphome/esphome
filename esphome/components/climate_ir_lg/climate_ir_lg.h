#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace climate_ir_lg {

// Temperature
const uint8_t TEMP_MIN = 18;  // Celsius
const uint8_t TEMP_MAX = 30;  // Celsius

class LgIrClimate : public climate_ir::ClimateIR {
 public:
  LgIrClimate()
      : climate_ir::ClimateIR(TEMP_MIN, TEMP_MAX, 1.0f, true, false,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override {
    send_swing_cmd_ = call.get_swing_mode().has_value();
    // swing resets after unit powered off
    if (call.get_mode().has_value() && *call.get_mode() == climate::CLIMATE_MODE_OFF)
      this->swing_mode = climate::CLIMATE_SWING_OFF;
    climate_ir::ClimateIR::control(call);
  }

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  /// Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;

  bool send_swing_cmd_{false};

  void calc_checksum_(uint32_t &value);
  void transmit_(uint32_t value);

  climate::ClimateMode mode_before_{climate::CLIMATE_MODE_OFF};
};

}  // namespace climate_ir_lg
}  // namespace esphome
