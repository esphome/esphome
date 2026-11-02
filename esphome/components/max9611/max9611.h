#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"

namespace esphome::max9611 {

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

class MAX9611Component final : public PollingComponent, public i2c::I2CDevice {
  SUB_SENSOR(voltage)
  SUB_SENSOR(current)
  SUB_SENSOR(watt)
  SUB_SENSOR(temp)

 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_current_resistor(float r) { current_resistor_ = r; }
  void set_gain(MAX9611Multiplexer g) { gain_ = g; }

 protected:
  float current_resistor_;
  uint8_t register_map_[0x0C];
  MAX9611Multiplexer gain_;
};

}  // namespace esphome::max9611
