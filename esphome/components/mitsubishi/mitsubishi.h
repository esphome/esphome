#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace mitsubishi {

// Temperature
const uint8_t MITSUBISHI_TEMP_MIN = 16;  // Celsius
const uint8_t MITSUBISHI_TEMP_MAX = 31;  // Celsius

class MitsubishiClimate : public climate_ir::ClimateIR {
 public:
  MitsubishiClimate()
      : climate_ir::ClimateIR(MITSUBISHI_TEMP_MIN, MITSUBISHI_TEMP_MAX, 1.0f, true, false,
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH},
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL}) {}

 protected:
  // Transmit via IR the state of this climate controller.
  void transmit_state() override;
  // Handle received IR Buffer
  bool on_receive(remote_base::RemoteReceiveData data) override;
  bool parse_state_frame_(const uint8_t frame[]);
};

}  // namespace mitsubishi
}  // namespace esphome
