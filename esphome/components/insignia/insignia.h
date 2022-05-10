#pragma once

#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/switch/switch.h"
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

  void set_sensor(sensor::Sensor *sensor) { this->sensor_ = sensor; }
  void set_fm_switch(switch_::Switch *fm_switch) { this->fm_switch_ = fm_switch; }
  void set_led_switch(switch_::Switch *led_switch) { this->led_switch_ = led_switch; }
  void set_fm_configured(bool b) { this->fm_configured_ = b; }
  void set_led_configured(bool b) { this->led_configured_ = b; }

 protected:
  /// Transmit via IR the state of this climate controller.
  void transmit_state() override;
  void setup() override;
  void dump_config() override;
  void update() override;
  void send_packet(uint8_t const *message, uint8_t length);
  void send_transmission(uint8_t const *message, uint8_t length);
  void toggle_led();
  uint8_t calculate_checksum(uint8_t const *message, uint8_t length);
  uint8_t reverse_bits(uint8_t inbyte);
  uint8_t compile_hvac_byte();
  uint8_t compile_set_point_byte();
  bool send_swing_cmd_{false};
  bool fm_configured_{false};
  bool fm_enabled_{false};
  bool fm_state_changed_{false};
  bool led_enabled_{false};
  bool led_configured_{false};
  switch_::Switch *fm_switch_{nullptr};
  switch_::Switch *led_switch_{nullptr};
};

}  // namespace insignia
}  // namespace esphome
