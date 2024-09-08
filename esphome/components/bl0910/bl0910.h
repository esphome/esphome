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
  // Sensor index is 1-based to match the BL0910 documentation
  // but the array is 0-based to save memory and simplify code
  void set_voltage_sensor(sensor::Sensor *voltage_sensor, int index, float voltage_reference) {
    this->voltage_sensor_[index - 1] = voltage_sensor;
    this->voltage_reference_[index - 1] = voltage_reference;
  }
  void set_current_sensor(sensor::Sensor *current_sensor, int index, float current_reference) {
    this->current_sensor_[index - 1] = current_sensor;
    this->current_reference_[index - 1] = current_reference;
  }
  void set_power_sensor(sensor::Sensor *power_sensor, int index, float power_reference) {
    this->power_sensor_[index - 1] = power_sensor;
    this->power_reference_[index - 1] = power_reference;
  }
  void set_energy_sensor(sensor::Sensor *energy_sensor, int index, float energy_reference) {
    this->energy_sensor_[index - 1] = energy_sensor;
    this->energy_reference_[index - 1] = energy_reference;
  }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor, int index) {
    this->power_factor_sensor_[index - 1] = power_factor_sensor;
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

 protected:
  void write_register(uint8_t addr, uint32_t data) {
    return this->write_register(addr, (data >> 16) & 0xFF, (data >> 8) & 0xFF, data & 0xFF);
  }
  void write_register(uint8_t addr, uint8_t data_h, uint8_t data_m, uint8_t data_l);
  int32_t read_register(uint8_t addr);
  float get_voltage(uint8_t channel);
  float get_frequency(void);
  float get_current(uint8_t channel);
  float get_power(uint8_t channel);
  float get_energy(uint8_t channel);
  float get_temperature(void);
  float get_powerfactor(uint8_t channel, float freq);
};
}  // namespace bl0910
}  // namespace esphome
