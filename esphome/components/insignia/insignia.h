#pragma once

#include "esphome/components/climate_ir/climate_ir.h"

namespace esphome {
namespace insignia {

// Supported temperature range in celcius
const float INSIGNIA_TEMP_MIN = 16.67;  // 62 Farhenheit
const float INSIGNIA_TEMP_MAX = 30.0;   // 86 Farhenheit

class InsigniaClimate : public climate_ir::ClimateIR {
 public:
  InsigniaClimate()
      : climate_ir::ClimateIR(INSIGNIA_TEMP_MIN, 
                              INSIGNIA_TEMP_MAX, 
                              1.0f,   // Step
                              true,   // Supports Dry
                              true,   // Supports Fan
                              {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM,
                               climate::CLIMATE_FAN_HIGH}, // Supported Fan Modes
                              {climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_VERTICAL} // Supported Swing Modes
                              ) {}

  /// Override control to change settings of the climate device.
  void control(const climate::ClimateCall &call) override {
    send_swing_cmd_ = call.get_swing_mode().has_value();
    climate_ir::ClimateIR::control(call);
  }

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  void send_packet(uint8_t const *message, uint8_t length);
  uint8_t reverse_bits(uint8_t inbyte);
  bool send_swing_cmd_{false};
};

}  // namespace insignia
}  // namespace esphome
