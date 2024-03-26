#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

#include <cinttypes>

namespace esphome {
namespace nau7802 {

enum NAU7802Gain {
  NAU7802_GAIN_128 = 0b111,
  NAU7802_GAIN_64 = 0b110,
  NAU7802_GAIN_32 = 0b101,
  NAU7802_GAIN_16 = 0b100,
  NAU7802_GAIN_8 = 0b011,
  NAU7802_GAIN_4 = 0b010,
  NAU7802_GAIN_2 = 0b001,
  NAU7802_GAIN_1 = 0b000,
};

enum NAU7802SPS {
  NAU7802_SPS_320 = 0b111,
  NAU7802_SPS_80 = 0b011,
  NAU7802_SPS_40 = 0b010,
  NAU7802_SPS_20 = 0b001,
  NAU7802_SPS_10 = 0b000,
};

enum NAU7802LDO {
  NAU7802_LDO_2V4 = 0b111,
  NAU7802_LDO_2V7 = 0b110,
  NAU7802_LDO_3V0 = 0b101,
  NAU7802_LDO_3V3 = 0b100,
  NAU7802_LDO_3V6 = 0b011,
  NAU7802_LDO_3V9 = 0b010,
  NAU7802_LDO_4V2 = 0b001,
  NAU7802_LDO_4V5 = 0b000,
  // Never write this to a register
  NAU7802_LDO_EXTERNAL = 0b1000,
};

enum NAU7802Calibration {
  NAU7802_CALIBRATION_NONE = 0b111,
  NAU7802_CALIBRATION_EXTERNAL_OFFSET = 0b10,
  NAU7802_CALIBRATION_INTERNAL_OFFSET = 0b00,
};

class NAU7802Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void set_samples_per_second(NAU7802SPS sps) { sps_ = sps; }
  void set_ldo_voltage(NAU7802LDO ldo) { ldo_ = ldo; }
  void set_gain(NAU7802Gain gain) { gain_ = gain; }
  void set_calibration(NAU7802Calibration calibration) { calibration_ = calibration; }

  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

 protected:
  NAU7802LDO ldo_;
  NAU7802SPS sps_;
  NAU7802Gain gain_;
  NAU7802Calibration calibration_;
  bool is_data_ready_();
};

}  // namespace nau7802
}  // namespace esphome
