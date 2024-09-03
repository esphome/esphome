#pragma once

#include "esphome/core/component.h"
#include "esphome/components/spi/spi.h"

#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace bl0910 {

// Mode 1 Trailing clock CPHA=1, Polarity low CPOL=0

#define NUM_CHANNELS 11

class BL0910 : public PollingComponent,
               public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_LOW, spi::CLOCK_PHASE_TRAILING,
                                     spi::DATA_RATE_1MHZ> {
 public:
  void set_voltage_sensor(sensor::Sensor *voltage_sensor_, int index, float uref_) {
    voltage_sensor[index - 1] = voltage_sensor_;
    uref[index - 1] = uref_;
  }
  void set_current_sensor(sensor::Sensor *current_sensor_, int index, float iref_) {
    current_sensor[index - 1] = current_sensor_;
    iref[index - 1] = iref_;
  }
  void set_power_sensor(sensor::Sensor *power_sensor_, int index, float pref_) {
    power_sensor[index - 1] = power_sensor_;
    pref[index - 1] = pref_;
  }
  void set_energy_sensor(sensor::Sensor *energy_sensor_, int index, float eref_) {
    energy_sensor[index - 1] = energy_sensor_;
    eref[index - 1] = eref_;
  }
  void set_power_factor_sensor(sensor::Sensor *power_factor_sensor_, int index) {
    power_factor_sensor[index - 1] = power_factor_sensor_;
  }
  void set_frequency_sensor(sensor::Sensor *frequency_sensor_) { frequency_sensor = frequency_sensor_; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor_) { temperature_sensor = temperature_sensor_; }

  void loop() override;

  void update() override;
  void setup() override;
  void dump_config() override;

 protected:
  sensor::Sensor *voltage_sensor[NUM_CHANNELS] = {};
  sensor::Sensor *current_sensor[NUM_CHANNELS] = {};
  // NB This may be negative as the circuits is seemingly able to measure
  // power in both directions
  sensor::Sensor *power_sensor[NUM_CHANNELS] = {};
  sensor::Sensor *energy_sensor[NUM_CHANNELS] = {};
  sensor::Sensor *power_factor_sensor[NUM_CHANNELS] = {};
  sensor::Sensor *frequency_sensor{nullptr};
  sensor::Sensor *temperature_sensor{nullptr};

  float uref[NUM_CHANNELS] = {};
  float iref[NUM_CHANNELS] = {};
  float pref[NUM_CHANNELS] = {};
  float eref[NUM_CHANNELS] = {};

 private:
  int8_t checksum_calc(uint8_t *data);
  void write_register(uint8_t addr, uint32_t data) {
    return this->write_register(addr, (data >> 16) & 0xFF, (data >> 8) & 0xFF, data & 0xFF);
  }
  void write_register(uint8_t addr, uint8_t data_h, uint8_t data_m, uint8_t data_l);
  int32_t read_register(uint8_t addr);
  float getVoltage(uint8_t channel);
  float getFreq(void);
  float getCurrent(uint8_t channel);
  float getPower(uint8_t channel);
  float getEnergy(uint8_t channel);
  float getTemperature(void);
  float getPowerFactor(uint8_t channel, float freq);
};
}  // namespace bl0910
}  // namespace esphome
