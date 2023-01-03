#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace max9611 {

enum MAX9611Multiplexer {
  MAX9611_MULTIPLEXER_CSA_GAIN1 = 0b000,
  MAX9611_MULTIPLEXER_CSA_GAIN4 = 0b001,
  MAX9611_MULTIPLEXER_CSA_GAIN8 = 0b010,
  MAX9611_MULTIPLEXER_RS = 0b011,
  MAX9611_MULTIPLEXER_OUT = 0b100,
  MAX9611_MULTIPLEXER_SET = 0b101,
  MAX9611_MULTIPLEXER_TEMP = 0b110,
  MAX9611_MULTIPLEXER_FAST_MODE = 0b111,
};

enum MAX9611RegisterMap {
  CSA_DATA_BYTE_MSB_ADRR = 0x00,
  CSA_DATA_BYTE_LSB_ADRR = 0x01,
  RS_DATA_BYTE_MSB_ADRR = 0x02,
  RS_DATA_BYTE_LSB_ADRR = 0x03,
  OUT_DATA_BYTE_MSB_ADRR = 0x04,  // Unused Op-Amp
  OUT_DATA_BYTE_LSB_ADRR = 0x05,  // Unused Op-Amp
  SET_DATA_BYTE_MSB_ADRR = 0x06,  // Unused Op-Amp
  SET_DATA_BYTE_LSB_ADRR = 0x07,  // Unused Op-Amp
  TEMP_DATA_BYTE_MSB_ADRR = 0x08,
  TEMP_DATA_BYTE_LSB_ADRR = 0x09,
  CONTROL_REGISTER_1_ADRR = 0x0A,
  CONTROL_REGISTER_2_ADRR = 0x0B,
};

class MAX9611Component : public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void update() override;
  void set_voltage_sensor(sensor::Sensor *vs) { voltage_sensor_ = vs; }
  void set_current_sensor(sensor::Sensor *cs) { current_sensor_ = cs; }
  void set_watt_sensor(sensor::Sensor *ws) { watt_sensor_ = ws; }
  void set_temp_sensor(sensor::Sensor *ts) { temperature_sensor_ = ts; }

  void set_current_resistor(float r) { current_resistor_ = r; }
  void set_gain(MAX9611Multiplexer g) { gain_ = g; }

 protected:
  sensor::Sensor *voltage_sensor_{nullptr};
  sensor::Sensor *current_sensor_{nullptr};
  sensor::Sensor *watt_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  float current_resistor_;
  uint8_t register_map_[0x0C];
  MAX9611Multiplexer gain_;
};

}  // namespace max9611
}  // namespace esphome
