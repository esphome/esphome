// Based on this datasheet:
// https://www.mouser.ca/datasheet/2/678/AVGO_S_A0002854364_1-2574547.pdf

#pragma once

#include "esphome/components/i2c/i2c.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/core/component.h"

namespace esphome {
namespace apds9306 {

enum MeasurementBitWidth : uint8_t {
  MEASUREMENT_BIT_WIDTH_20 = 0,
  MEASUREMENT_BIT_WIDTH_19 = 1,
  MEASUREMENT_BIT_WIDTH_18 = 2,
  MEASUREMENT_BIT_WIDTH_17 = 3,
  MEASUREMENT_BIT_WIDTH_16 = 4,
  MEASUREMENT_BIT_WIDTH_13 = 5,
};
static const uint8_t MEASUREMENT_BIT_WIDTH_VALUES[] = {20, 19, 18, 17, 16, 13};

enum MeasurementRate : uint8_t {
  MEASUREMENT_RATE_25 = 0,
  MEASUREMENT_RATE_50 = 1,
  MEASUREMENT_RATE_100 = 2,
  MEASUREMENT_RATE_200 = 3,
  MEASUREMENT_RATE_500 = 4,
  MEASUREMENT_RATE_1000 = 5,
  MEASUREMENT_RATE_2000 = 6,
};
static const uint16_t MEASUREMENT_RATE_VALUES[] = {25, 50, 100, 200, 500, 1000, 2000};

enum AmbientLightGain : uint8_t {
  AMBIENT_LIGHT_GAIN_1 = 0,
  AMBIENT_LIGHT_GAIN_3 = 1,
  AMBIENT_LIGHT_GAIN_6 = 2,
  AMBIENT_LIGHT_GAIN_9 = 3,
  AMBIENT_LIGHT_GAIN_18 = 4,
};
static const uint8_t AMBIENT_LIGHT_GAIN_VALUES[] = {1, 3, 6, 9, 18};

class APDS9306 : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::BUS; }
  void dump_config() override;
  void update() override;
  void set_bit_width(MeasurementBitWidth bit_width) { this->bit_width_ = bit_width; }
  void set_measurement_rate(MeasurementRate measurement_rate) { this->measurement_rate_ = measurement_rate; }
  void set_ambient_light_gain(AmbientLightGain gain) { this->gain_ = gain; }

 protected:
  enum ErrorCode {
    NONE = 0,
    COMMUNICATION_FAILED,
    WRONG_ID,
  } error_code_{NONE};

  MeasurementBitWidth bit_width_;
  MeasurementRate measurement_rate_;
  AmbientLightGain gain_;
};

}  // namespace apds9306
}  // namespace esphome
