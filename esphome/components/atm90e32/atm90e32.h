#pragma once

#include <unordered_map>
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
  const char *phase_labels[3] = {"A", "B", "C"};
  // these registers are not sucessive, so we can't just do 'base + phase'
  const uint16_t voltage_gain_registers[3] = {ATM90E32_REGISTER_UGAINA, ATM90E32_REGISTER_UGAINB,
                                              ATM90E32_REGISTER_UGAINC};
  const uint16_t current_gain_registers[3] = {ATM90E32_REGISTER_IGAINA, ATM90E32_REGISTER_IGAINB,
                                              ATM90E32_REGISTER_IGAINC};
  const uint16_t voltage_offset_registers[3] = {ATM90E32_REGISTER_UOFFSETA, ATM90E32_REGISTER_UOFFSETB,
                                                ATM90E32_REGISTER_UOFFSETC};
  const uint16_t current_offset_registers[3] = {ATM90E32_REGISTER_IOFFSETA, ATM90E32_REGISTER_IOFFSETB,
                                                ATM90E32_REGISTER_IOFFSETC};
  const uint16_t power_offset_registers[3] = {ATM90E32_REGISTER_POFFSETA, ATM90E32_REGISTER_POFFSETB,
                                              ATM90E32_REGISTER_POFFSETC};
  const uint16_t reactive_power_offset_registers[3] = {ATM90E32_REGISTER_QOFFSETA, ATM90E32_REGISTER_QOFFSETB,
                                                       ATM90E32_REGISTER_QOFFSETC};
  const uint16_t over_voltage_flags[3] = {ATM90E32_STATUS_S0_OVPHASEAST, ATM90E32_STATUS_S0_OVPHASEBST,
                                          ATM90E32_STATUS_S0_OVPHASECST};
  const uint16_t voltage_sag_flags[3] = {ATM90E32_STATUS_S1_SAGPHASEAST, ATM90E32_STATUS_S1_SAGPHASEBST,
                                         ATM90E32_STATUS_S1_SAGPHASECST};
  const uint16_t phase_loss_flags[3] = {ATM90E32_STATUS_S1_PHASELOSSAST, ATM90E32_STATUS_S1_PHASELOSSBST,
                                        ATM90E32_STATUS_S1_PHASELOSSCST};
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
  void set_voltage_offset(uint8_t phase, int16_t offset) { this->offset_phase_[phase].voltage_offset_ = offset; }
  void set_current_offset(uint8_t phase, int16_t offset) { this->offset_phase_[phase].current_offset_ = offset; }
  void set_active_power_offset(uint8_t phase, int16_t offset) {
    this->power_offset_phase_[phase].active_power_offset = offset;
  }
  void set_reactive_power_offset(uint8_t phase, int16_t offset) {
    this->power_offset_phase_[phase].reactive_power_offset = offset;
  }
  void set_freq_sensor(sensor::Sensor *freq_sensor) { freq_sensor_ = freq_sensor; }
  void set_peak_current_signed(bool flag) { peak_current_signed_ = flag; }
  void set_chip_temperature_sensor(sensor::Sensor *chip_temperature_sensor) {
    chip_temperature_sensor_ = chip_temperature_sensor;
  }
  void set_line_freq(int freq) { line_freq_ = freq; }
  void set_current_phases(int phases) { current_phases_ = phases; }
  void set_pga_gain(uint16_t gain) { pga_gain_ = gain; }
  void run_offset_calibrations();
  void run_power_offset_calibrations();
  void clear_offset_calibrations();
  void clear_power_offset_calibrations();
  void clear_gain_calibrations();
  void set_enable_offset_calibration(bool flag) { enable_offset_calibration_ = flag; }
  void set_enable_gain_calibration(bool flag) { enable_gain_calibration_ = flag; }
  int16_t calibrate_offset(uint8_t phase, bool voltage);
  int16_t calibrate_power_offset(uint8_t phase, bool reactive);
  void run_gain_calibrations();
#ifdef USE_NUMBER
  void set_reference_voltage(uint8_t phase, number::Number *ref_voltage) { ref_voltages_[phase] = ref_voltage; }
  void set_reference_current(uint8_t phase, number::Number *ref_current) { ref_currents_[phase] = ref_current; }
#endif
  float get_reference_voltage(uint8_t phase) {
#ifdef USE_NUMBER
    return (phase >= 0 && phase < 3 && ref_voltages_[phase]) ? ref_voltages_[phase]->state : 120.0;  // Default voltage
#else
    return 120.0;  // Default voltage
#endif
  }
  float get_reference_current(uint8_t phase) {
#ifdef USE_NUMBER
    return (phase >= 0 && phase < 3 && ref_currents_[phase]) ? ref_currents_[phase]->state : 5.0f;  // Default current
#else
    return 5.0f;   // Default current
#endif
  }
  bool using_saved_calibrations_ = false;  // Track if stored calibrations are being used
#ifdef USE_TEXT_SENSOR
  void check_phase_status();
  void check_freq_status();
  void check_over_current();
  void set_phase_status_text_sensor(uint8_t phase, text_sensor::TextSensor *sensor) {
    this->phase_status_text_sensor_[phase] = sensor;
  }
  void set_freq_status_text_sensor(text_sensor::TextSensor *sensor) { this->freq_status_text_sensor_ = sensor; }
#endif
  uint16_t calculate_voltage_threshold(int line_freq, uint16_t ugain, float multiplier);
  int32_t last_periodic_millis = millis();

 protected:
#ifdef USE_NUMBER
  number::Number *ref_voltages_[3]{nullptr, nullptr, nullptr};
  number::Number *ref_currents_[3]{nullptr, nullptr, nullptr};
#endif
  uint16_t read16_(uint16_t a_register);
  int read32_(uint16_t addr_h, uint16_t addr_l);
  void write16_(uint16_t a_register, uint16_t val);
  float get_local_phase_voltage_(uint8_t phase);
  float get_local_phase_current_(uint8_t phase);
  float get_local_phase_active_power_(uint8_t phase);
  float get_local_phase_reactive_power_(uint8_t phase);
  float get_local_phase_apparent_power_(uint8_t phase);
  float get_local_phase_power_factor_(uint8_t phase);
  float get_local_phase_forward_active_energy_(uint8_t phase);
  float get_local_phase_reverse_active_energy_(uint8_t phase);
  float get_local_phase_angle_(uint8_t phase);
  float get_local_phase_harmonic_active_power_(uint8_t phase);
  float get_local_phase_peak_current_(uint8_t phase);
  float get_phase_voltage_(uint8_t phase);
  float get_phase_voltage_avg_(uint8_t phase);
  float get_phase_current_(uint8_t phase);
  float get_phase_current_avg_(uint8_t phase);
  float get_phase_active_power_(uint8_t phase);
  float get_phase_reactive_power_(uint8_t phase);
  float get_phase_apparent_power_(uint8_t phase);
  float get_phase_power_factor_(uint8_t phase);
  float get_phase_forward_active_energy_(uint8_t phase);
  float get_phase_reverse_active_energy_(uint8_t phase);
  float get_phase_angle_(uint8_t phase);
  float get_phase_harmonic_active_power_(uint8_t phase);
  float get_phase_peak_current_(uint8_t phase);
  float get_frequency_();
  float get_chip_temperature_();
  bool get_publish_interval_flag_() { return publish_interval_flag_; };
  void set_publish_interval_flag_(bool flag) { publish_interval_flag_ = flag; };
  void restore_offset_calibrations_();
  void restore_power_offset_calibrations_();
  void restore_gain_calibrations_();
  void save_gain_calibration_to_memory_();
  void write_offsets_to_registers_(uint8_t phase, int16_t voltage_offset, int16_t current_offset);
  void write_power_offsets_to_registers_(uint8_t phase, int16_t p_offset, int16_t q_offset);
  void write_gains_to_registers_();
  bool verify_gain_writes_();
  bool validate_spi_read_(uint16_t expected, const char *context = nullptr);

  struct ATM90E32Phase {
    uint16_t voltage_gain_{0};
    uint16_t ct_gain_{0};
    int16_t voltage_offset_{0};
    int16_t current_offset_{0};
    int16_t active_power_offset_{0};
    int16_t reactive_power_offset_{0};
    float voltage_{0};
    float current_{0};
    float active_power_{0};
    float reactive_power_{0};
    float apparent_power_{0};
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

  struct OffsetCalibration {
    int16_t voltage_offset_{0};
    int16_t current_offset_{0};
  } offset_phase_[3];

  struct PowerOffsetCalibration {
    int16_t active_power_offset{0};
    int16_t reactive_power_offset{0};
  } power_offset_phase_[3];

  struct GainCalibration {
    uint16_t voltage_gain{1};
    uint16_t current_gain{1};
  } gain_phase_[3];

  ESPPreferenceObject offset_pref_;
  ESPPreferenceObject power_offset_pref_;
  ESPPreferenceObject gain_calibration_pref_;

  sensor::Sensor *freq_sensor_{nullptr};
#ifdef USE_TEXT_SENSOR
  text_sensor::TextSensor *phase_status_text_sensor_[3]{nullptr};
  text_sensor::TextSensor *freq_status_text_sensor_{nullptr};
#endif
  sensor::Sensor *chip_temperature_sensor_{nullptr};
  uint16_t pga_gain_{0x15};
  int line_freq_{60};
  int current_phases_{3};
  bool publish_interval_flag_{false};
  bool peak_current_signed_{false};
  bool enable_offset_calibration_{false};
  bool enable_gain_calibration_{false};
};

}  // namespace atm90e32
}  // namespace esphome
