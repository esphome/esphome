#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::veml3235 {

// Register IDs/locations
//
static const uint8_t CONFIG_REG = 0x00;
static const uint8_t W_REG = 0x04;
static const uint8_t ALS_REG = 0x05;
static const uint8_t ID_REG = 0x09;

// Bit offsets within CONFIG_REG
//
// The device expects the low data byte first while write_byte_16() sends the high byte first, so the 16-bit
// configuration word used here is byte-swapped relative to the datasheet: datasheet low-byte bits are word
// bits 15:8 here and datasheet high-byte bits are word bits 7:0.
//
static const uint8_t CONFIG_REG_IT_BIT = 12;
static const uint8_t CONFIG_REG_DG_BIT = 5;
static const uint8_t CONFIG_REG_G_BIT = 3;

// Other important constants
//
static const uint8_t DEVICE_ID = 0x35;

// Resolution (lx/count) at maximum sensitivity (integration time 800 ms, gain 4x, digital gain 2x)
//
static const float LUX_MULTIPLIER_BASE = 0.00213f;

// Enum for conversion/integration time settings for the VEML3235.
//
// Specific values of the enum constants are register values taken from the VEML3235 datasheet.
// Longer times mean more accurate results, but will take more energy/more time.
//
enum VEML3235ComponentIntegrationTime : uint8_t {
  VEML3235_INTEGRATION_TIME_50MS = 0b000,
  VEML3235_INTEGRATION_TIME_100MS = 0b001,
  VEML3235_INTEGRATION_TIME_200MS = 0b010,
  VEML3235_INTEGRATION_TIME_400MS = 0b011,
  VEML3235_INTEGRATION_TIME_800MS = 0b100,
};

// Enum for digital gain settings for the VEML3235.
// Higher values are better for low light situations, but can increase noise.
//
enum VEML3235ComponentDigitalGain : uint8_t {
  VEML3235_DIGITAL_GAIN_1X = 0b0,
  VEML3235_DIGITAL_GAIN_2X = 0b1,
};

// Enum for gain settings for the VEML3235.
// Higher values are better for low light situations, but can increase noise.
//
enum VEML3235ComponentGain : uint8_t {
  VEML3235_GAIN_1X = 0b00,
  VEML3235_GAIN_2X = 0b01,
  VEML3235_GAIN_4X = 0b11,
};

class VEML3235Sensor final : public sensor::Sensor, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  // Used by ESPHome framework. Does NOT actually set the value on the device.
  void set_auto_gain(bool auto_gain) { this->auto_gain_ = auto_gain; }
  void set_auto_gain_threshold_high(float auto_gain_threshold_high) {
    this->auto_gain_threshold_high_ = auto_gain_threshold_high;
  }
  void set_auto_gain_threshold_low(float auto_gain_threshold_low) {
    this->auto_gain_threshold_low_ = auto_gain_threshold_low;
  }
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
  bool refresh_config_reg();

 protected:
  // One measurement pass: reads the ALS counts, possibly adjusts the sensitivity and schedules a re-read,
  // otherwise publishes the result
  void read_and_publish_(uint8_t adjustments_left);
  // Chooses a new sensitivity for the given ALS reading and writes it to the device.
  // Returns true only if the device configuration was changed.
  bool adjust_sensitivity_(uint16_t counts);
  float counts_to_lux_(uint16_t counts) const;

  // Overall sensitivity multiplier (1x-128x, always a power of two) relative to the least sensitive
  // configuration (integration time 50 ms, gain 1x, digital gain 1x). ALS counts scale linearly with it.
  uint16_t sensitivity_factor_() const;
  void set_sensitivity_factor_(uint16_t factor);
  uint8_t gain_factor_() const;
  uint16_t integration_time_ms_() const { return 50 << this->integration_time_; }

  // Members are ordered largest to smallest to minimize padding
  float auto_gain_threshold_high_{0.9};
  float auto_gain_threshold_low_{0.2};
  VEML3235ComponentDigitalGain digital_gain_{VEML3235_DIGITAL_GAIN_1X};
  VEML3235ComponentGain gain_{VEML3235_GAIN_1X};
  VEML3235ComponentIntegrationTime integration_time_{VEML3235_INTEGRATION_TIME_50MS};
  bool auto_gain_{true};
  bool measurement_in_progress_{false};
};

}  // namespace esphome::veml3235
