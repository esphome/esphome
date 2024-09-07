#pragma once

#include "atm90e32_reg.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/spi/spi.h"
#include "esphome/core/application.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace atm90e32 {

class ATM90E32Component : public PollingComponent,
                          public spi::SPIDevice<spi::BIT_ORDER_MSB_FIRST, spi::CLOCK_POLARITY_HIGH,
                                                spi::CLOCK_PHASE_TRAILING, spi::DATA_RATE_1MHZ> {
 public:
  static const uint8_t PHASEA = 0;
  static const uint8_t PHASEB = 1;
  static const uint8_t PHASEC = 2;
  void loop() override;
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override;
  void update() override;
  void set_voltage_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].voltage_sensor_ = obj; }
  void set_current_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].current_sensor_ = obj; }
  void set_power_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].power_sensor_ = obj; }
  void set_reactive_power_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].reactive_power_sensor_ = obj; }
  void set_apparent_power_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].apparent_power_sensor_ = obj; }
  void set_forward_active_energy_sensor(int phase, sensor::Sensor *obj) {
    this->phase_[phase].forward_active_energy_sensor_ = obj;
  }
  void set_reverse_active_energy_sensor(int phase, sensor::Sensor *obj) {
    this->phase_[phase].reverse_active_energy_sensor_ = obj;
  }
  void set_power_factor_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].power_factor_sensor_ = obj; }
  void set_phase_angle_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].phase_angle_sensor_ = obj; }
  void set_harmonic_active_power_sensor(int phase, sensor::Sensor *obj) {
    this->phase_[phase].harmonic_active_power_sensor_ = obj;
  }
  void set_peak_current_sensor(int phase, sensor::Sensor *obj) { this->phase_[phase].peak_current_sensor_ = obj; }
  void set_volt_gain(int phase, uint16_t gain) { this->phase_[phase].voltage_gain_ = gain; }
  void set_ct_gain(int phase, uint16_t gain) { this->phase_[phase].ct_gain_ = gain; }
  void set_freq_sensor(sensor::Sensor *freq_sensor) { freq_sensor_ = freq_sensor; }
  void set_peak_current_signed(bool flag) { peak_current_signed_ = flag; }
  void set_chip_temperature_sensor(sensor::Sensor *chip_temperature_sensor) {
    chip_temperature_sensor_ = chip_temperature_sensor;
  }
  void set_line_freq(int freq) { line_freq_ = freq; }
  void set_current_phases(int phases) { current_phases_ = phases; }
  void set_pga_gain(uint16_t gain) { pga_gain_ = gain; }
  void run_offset_calibrations();
  void clear_offset_calibrations();
  void set_enable_offset_calibration(bool flag) { enable_offset_calibration_ = flag; }
  uint16_t calibrate_voltage_offset_phase(uint8_t /*phase*/);
  uint16_t calibrate_current_offset_phase(uint8_t /*phase*/);
  int32_t last_periodic_millis = millis();

 protected:
  uint16_t read16_(uint16_t a_register);
  int read32_(uint16_t addr_h, uint16_t addr_l);
  void write16_(uint16_t a_register, uint16_t val);
  float get_local_phase_voltage_(uint8_t /*phase*/);
  float get_local_phase_current_(uint8_t /*phase*/);
  float get_local_phase_active_power_(uint8_t /*phase*/);
  float get_local_phase_reactive_power_(uint8_t /*phase*/);
  float get_local_phase_power_factor_(uint8_t /*phase*/);
  float get_local_phase_forward_active_energy_(uint8_t /*phase*/);
  float get_local_phase_reverse_active_energy_(uint8_t /*phase*/);
  float get_local_phase_angle_(uint8_t /*phase*/);
  float get_local_phase_harmonic_active_power_(uint8_t /*phase*/);
  float get_local_phase_peak_current_(uint8_t /*phase*/);
  float get_phase_voltage_(uint8_t /*phase*/);
  float get_phase_voltage_avg_(uint8_t /*phase*/);
  float get_phase_current_(uint8_t /*phase*/);
  float get_phase_current_avg_(uint8_t /*phase*/);
  float get_phase_active_power_(uint8_t /*phase*/);
  float get_phase_reactive_power_(uint8_t /*phase*/);
  float get_phase_power_factor_(uint8_t /*phase*/);
  float get_phase_forward_active_energy_(uint8_t /*phase*/);
  float get_phase_reverse_active_energy_(uint8_t /*phase*/);
  float get_phase_angle_(uint8_t /*phase*/);
  float get_phase_harmonic_active_power_(uint8_t /*phase*/);
  float get_phase_peak_current_(uint8_t /*phase*/);
  float get_frequency_();
  float get_chip_temperature_();
  bool get_publish_interval_flag_() { return publish_interval_flag_; };
  void set_publish_interval_flag_(bool flag) { publish_interval_flag_ = flag; };
  void restore_calibrations_();

  struct ATM90E32Phase {
    uint16_t voltage_gain_{0};
    uint16_t ct_gain_{0};
    uint16_t voltage_offset_{0};
    uint16_t current_offset_{0};
    float voltage_{0};
    float current_{0};
    float active_power_{0};
    float reactive_power_{0};
    float power_factor_{0};
    float forward_active_energy_{0};
    float reverse_active_energy_{0};
    float phase_angle_{0};
    float harmonic_active_power_{0};
    float peak_current_{0};
    sensor::Sensor *voltage_sensor_{nullptr};
    sensor::Sensor *current_sensor_{nullptr};
    sensor::Sensor *power_sensor_{nullptr};
    sensor::Sensor *reactive_power_sensor_{nullptr};
    sensor::Sensor *apparent_power_sensor_{nullptr};
    sensor::Sensor *power_factor_sensor_{nullptr};
    sensor::Sensor *forward_active_energy_sensor_{nullptr};
    sensor::Sensor *reverse_active_energy_sensor_{nullptr};
    sensor::Sensor *phase_angle_sensor_{nullptr};
    sensor::Sensor *harmonic_active_power_sensor_{nullptr};
    sensor::Sensor *peak_current_sensor_{nullptr};
    uint32_t cumulative_forward_active_energy_{0};
    uint32_t cumulative_reverse_active_energy_{0};
  } phase_[3];

  struct Calibration {
    uint16_t voltage_offset_{0};
    uint16_t current_offset_{0};
  } offset_phase_[3];

  ESPPreferenceObject pref_;

  sensor::Sensor *freq_sensor_{nullptr};
  sensor::Sensor *chip_temperature_sensor_{nullptr};
  uint16_t pga_gain_{0x15};
  int line_freq_{60};
  int current_phases_{3};
  bool publish_interval_flag_{false};
  bool peak_current_signed_{false};
  bool enable_offset_calibration_{false};
};

}  // namespace atm90e32
}  // namespace esphome
