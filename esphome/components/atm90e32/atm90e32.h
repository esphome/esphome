#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#define PHASEA 0
#define PHASEB 1
#define PHASEC 2


namespace esphome {
namespace atm90e32 {

class ATM90E32Component : public PollingComponent,
                          public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                                spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_2MHZ> {
 public:
  void loop() override;
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;

  void set_voltage_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].voltage_sensor_ = obj; }
  void set_current_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].current_sensor_ = obj; }
  void set_power_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].power_sensor_ = obj; }
  void set_reactive_power_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].reactive_power_sensor_ = obj; }
  void set_forward_active_energy_sensor(int phase, sensor::Sensor *obj) {
    this->phase_[phase].forward_active_energy_sensor_ = obj;
  }
  void set_reverse_active_energy_sensor(int phase, sensor::Sensor *obj) {
    this->phase_[phase].reverse_active_energy_sensor_ = obj;
  }
  void set_power_factor_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].power_factor_sensor_ = obj; }
  void set_volt_gain(int phase, uint16_t gain) { this->phase_[phase].voltage_gain_ = gain; }
  void set_offset_voltage(int phase, uint16_t offset) { this->phase_[phase].voltage_offset_ = offset; }
  void set_ct_gain(int phase, uint16_t gain) { this->phase_[phase].ct_gain_ = gain; }

  void set_freq_sensor(sensor::Sensor *freq_sensor) { freq_sensor_ = freq_sensor; }
  void set_chip_temperature_sensor(sensor::Sensor *chip_temperature_sensor) {
    chip_temperature_sensor_ = chip_temperature_sensor;
  }
  void set_line_freq(int freq) { line_freq_ = freq; }
  void set_current_phases(int phases) { current_phases_ = phases; }
  void set_pga_gain(uint16_t gain) { pga_gain_ = gain; }
    // Function to calibrate the voltage offset for phase A
  uint16_t calibrateVoltageOffsetPhase(uint8_t phase) {
    uint8_t numReads = 5;
    uint32_t totalValue = 0;
    for (int i = 0; i < numReads; ++i) {
      uint32_t measurementValue = get_line_voltage_(phase);
        totalValue += measurementValue;
      }
    uint32_t averageValue = totalValue / numReads;
    uint32_t shiftedValue = averageValue >> 7;
    uint32_t voltageOffset = ~shiftedValue + 1;
    return voltageOffset & 0xFFFF; // Take the lower 16 bits
  }
  uint16_t calibrateCurrentOffsetPhase(uint8_t phase) {
    uint8_t numReads = 5;
    uint32_t totalValue = 0;
    for (int i = 0; i < numReads; ++i) {
      uint32_t measurementValue = get_line_current_(phase);
      totalValue += measurementValue;
    }
    uint32_t averageValue = totalValue / numReads;
    uint32_t currentOffset = ~averageValue + 1;
    return currentOffset & 0xFFFF; // Take the lower 16 bits
  }

  int32_t last_periodic_millis = millis();

 protected:
  uint16_t read16_(uint16_t a_register);
  int read32_(uint16_t addr_h, uint16_t addr_l);
  void write16_(uint16_t a_register, uint16_t val);
  float get_phase_voltage_(uint8_t);
  float get_phase_current_(uint8_t);
  float get_line_voltage_(uint8_t);
  float get_line_voltage_avg(uint8_t);
  float get_line_current_(uint8_t);
  float get_line_current_avg(uint8_t);
  float get_active_power_a_();
  float get_active_power_b_();
  float get_active_power_c_();
  float get_reactive_power_a_();
  float get_reactive_power_b_();
  float get_reactive_power_c_();
  float get_power_factor_a_();
  float get_power_factor_b_();
  float get_power_factor_c_();
  float get_forward_active_energy_a_();
  float get_forward_active_energy_b_();
  float get_forward_active_energy_c_();
  float get_reverse_active_energy_a_();
  float get_reverse_active_energy_b_();
  float get_reverse_active_energy_c_();
  float get_frequency_();
  float get_chip_temperature_();

  struct ATM90E32Phase {
    uint16_t voltage_gain_{7305};
    uint16_t ct_gain_{27961};
    float voltage_ {0};
    float current_ {0};
    uint16_t voltage_offset_ {0};
    uint16_t current_offset_ {0};
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *power_sensor_{nullptr};
    sensor::Sensor *reactive_power_sensor_{nullptr};
    sensor::Sensor *power_factor_sensor_{nullptr};
    sensor::Sensor *forward_active_energy_sensor_{nullptr};
    sensor::Sensor *reverse_active_energy_sensor_{nullptr};
    uint32_t cumulative_forward_active_energy_{0};
    uint32_t cumulative_reverse_active_energy_{0};
  } phase_[3];
  sensor::Sensor *freq_sensor_{nullptr};
  sensor::Sensor *chip_temperature_sensor_{nullptr};
  uint16_t pga_gain_{0x15};
  int line_freq_{60};
  int current_phases_{3};
};

}  // namespace atm90e32
}  // namespace esphome
