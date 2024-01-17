#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome {
namespace veml3235 {

// Register IDs/locations
//
static const uint8_t CONFIG_REG = 0x00;
static const uint8_t W_REG = 0x04;
static const uint8_t ALS_REG = 0x05;
static const uint8_t ID_REG = 0x09;

// Bit offsets within CONFIG_REG
//
static const uint8_t CONFIG_REG_IT_BIT = 12;
static const uint8_t CONFIG_REG_DG_BIT = 5;
static const uint8_t CONFIG_REG_G_BIT = 3;

// Other important constants
//
static const uint8_t DEVICE_ID = 0x35;
static const uint16_t SHUTDOWN_BITS = 0x0018;

// Base multiplier value for lux computation
//
static const float LUX_MULTIPLIER_BASE = 0.00213;

// Enum for conversion/integration time settings for the VEML3235.
//
// Specific values of the enum constants are register values taken from the VEML3235 datasheet.
// Longer times mean more accurate results, but will take more energy/more time.
//
enum VEML3235ComponentIntegrationTime {
  VEML3235_INTEGRATION_TIME_50MS = 0b000,
  VEML3235_INTEGRATION_TIME_100MS = 0b001,
  VEML3235_INTEGRATION_TIME_200MS = 0b010,
  VEML3235_INTEGRATION_TIME_400MS = 0b011,
  VEML3235_INTEGRATION_TIME_800MS = 0b100,
};

// Enum for digital gain settings for the VEML3235.
// Higher values are better for low light situations, but can increase noise.
//
enum VEML3235ComponentDigitalGain {
  VEML3235_DIGITAL_GAIN_1X = 0b0,
  VEML3235_DIGITAL_GAIN_2X = 0b1,
};

// Enum for gain settings for the VEML3235.
// Higher values are better for low light situations, but can increase noise.
//
enum VEML3235ComponentGain {
  VEML3235_GAIN_1X = 0b00,
  VEML3235_GAIN_2X = 0b01,
  VEML3235_GAIN_4X = 0b11,
};

class VEML3235Sensor : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override { this->publish_state(this->read_lx_()); }
  float get_setup_priority() const override { return setup_priority::DATA; }

  // Used by ESPHome framework. Does NOT actually set the value on the device.
  void set_auto_gain(bool auto_gain) { this->auto_gain_ = auto_gain; }
  void set_auto_gain_threshold_high(float auto_gain_threshold_high) {
    this->auto_gain_threshold_high_ = auto_gain_threshold_high;
  }
  void set_auto_gain_threshold_low(float auto_gain_threshold_low) {
    this->auto_gain_threshold_low_ = auto_gain_threshold_low;
  }
  void set_power_on(bool power_on) { this->power_on_ = power_on; }
  void set_digital_gain(VEML3235ComponentDigitalGain digital_gain) { this->digital_gain_ = digital_gain; }
  void set_gain(VEML3235ComponentGain gain) { this->gain_ = gain; }
  void set_integration_time(VEML3235ComponentIntegrationTime integration_time) {
    this->integration_time_ = integration_time;
  }

  bool auto_gain() { return this->auto_gain_; }
  float auto_gain_threshold_high() { return this->auto_gain_threshold_high_; }
  float auto_gain_threshold_low() { return this->auto_gain_threshold_low_; }
  VEML3235ComponentDigitalGain digital_gain() { return this->digital_gain_; }
  VEML3235ComponentGain gain() { return this->gain_; }
  VEML3235ComponentIntegrationTime integration_time() { return this->integration_time_; }

  // Updates the configuration register on the device
  bool refresh_config_reg(bool force_on = false);

 protected:
  float read_lx_();
  void adjust_gain_(uint16_t als_raw_value);

  bool auto_gain_{true};
  bool power_on_{true};
  float auto_gain_threshold_high_{0.9};
  float auto_gain_threshold_low_{0.2};
  VEML3235ComponentDigitalGain digital_gain_{VEML3235_DIGITAL_GAIN_1X};
  VEML3235ComponentGain gain_{VEML3235_GAIN_1X};
  VEML3235ComponentIntegrationTime integration_time_{VEML3235_INTEGRATION_TIME_50MS};
};

}  // namespace veml3235
}  // namespace esphome
