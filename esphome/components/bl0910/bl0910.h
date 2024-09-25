#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"

#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bl0910 {

// Mode 1 Trailing clock CPHA=1, Polarity low CPOL=0

static const uint8_t NUM_CHANNELS = 11;

class BL0910 : public PollingComponent,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_TRAILING,
                                     spi::DATA_RATE_1MHZ> {
 public:
  // Sensor channel is 1-based to match the BL0910 documentation
  // but the array is 0-based to save memory and simplify code
  void set_voltage_sensor(sensor::Sensor *voltage_sensor, int channel, float voltage_reference) {
    this->voltage_sensor_[channel - 1] = voltage_sensor;
    this->voltage_reference_[channel - 1] = voltage_reference;
  }
  void set_current_sensor(sensor::Sensor *current_sensor, int channel, float current_reference) {
    this->current_sensor_[channel - 1] = current_sensor;
    this->current_reference_[channel - 1] = current_reference;
  }
  void set_power_sensor(sensor::Sensor *power_sensor, int channel, float power_reference) {
    this->power_sensor_[channel - 1] = power_sensor;
    this->power_reference_[channel - 1] = power_reference;
  }
  void set_energy_sensor(sensor::Sensor *energy_sensor, int channel, float energy_reference) {
    this->energy_sensor_[channel - 1] = energy_sensor;
    this->energy_reference_[channel - 1] = energy_reference;
  }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor, int channel) {
    this->power_factor_sensor_[channel - 1] = power_factor_sensor;
  }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor) { this->frequency_sensor_ = frequency_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }

  void loop() override;

  void update() override;
  void setup() override;
  void dump_config() override;

 protected:
  sensor::Sensor *voltage_sensor_[NUM_CHANNELS] = {};
  sensor::Sensor *current_sensor_[NUM_CHANNELS] = {};
  // NB This may be negative as the circuits is seemingly able to measure
  // power in both directions
  sensor::Sensor *power_sensor_[NUM_CHANNELS] = {};
  sensor::Sensor *energy_sensor_[NUM_CHANNELS] = {};
  sensor::Sensor *power_factor_sensor_[NUM_CHANNELS] = {};
  sensor::Sensor *frequency_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};

  float voltage_reference_[NUM_CHANNELS] = {};
  float current_reference_[NUM_CHANNELS] = {};
  float power_reference_[NUM_CHANNELS] = {};
  float energy_reference_[NUM_CHANNELS] = {};

  // Current channel being read
  uint8_t current_channel_ = 0;
  float freq_ = 50.0;

  void write_register_(uint8_t addr, uint32_t data) {
    return this->write_register_(addr, (data >> 16) & 0xFF, (data >> 8) & 0xFF, data & 0xFF);
  }
  void write_register_(uint8_t addr, uint8_t data_h, uint8_t data_m, uint8_t data_l);
  int32_t read_register_(uint8_t addr);
  uint32_t read_uregister_(uint8_t addr);
  float get_voltage_(uint8_t channel);
  float get_frequency_();
  float get_current_(uint8_t channel);
  float get_power_(uint8_t channel);
  float get_energy_(uint8_t channel);
  float get_temperature_();
  float get_powerfactor_(uint8_t channel, float freq);
};
}  // namespace bl0910
}  // namespace esphome
