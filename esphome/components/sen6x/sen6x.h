#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

#include <vector>

namespace esphome::sen6x {

enum ERRORCODE {
  COMMUNICATION_FAILED,
  SERIAL_NUMBER_IDENTIFICATION_FAILED,
  MEASUREMENT_INIT_FAILED,
  PRODUCT_NAME_FAILED,
  FIRMWARE_FAILED,
  UNKNOWN_ERROR
};

// Shortest time interval of 3H for storing baseline values.
// Prevents wear of the flash because of too many write operations
static constexpr uint32_t SHORTEST_BASELINE_STORE_INTERVAL_MS = 10800 * 1000UL;
// Store anyway if the baseline difference exceeds the max storage diff value
static constexpr uint32_t MAXIMUM_STORAGE_DIFF = 50;
// Default NOx std_initial value per Sensirion specification
static constexpr uint16_t NOX_DEFAULT_STD_INITIAL = 50;

struct Sen6xBaselines {
  int32_t state0;
  int32_t state1;
  uint32_t config_hash;  // Used to detect config/version changes and invalidate old baselines
} PACKED;                // NOLINT

struct GasTuning {
  uint16_t index_offset;
  uint16_t learning_time_offset_hours;
  uint16_t learning_time_gain_hours;
  uint16_t gating_max_duration_minutes;
  uint16_t std_initial;
  uint16_t gain_factor;
};

struct TemperatureCompensation {
  int16_t offset;
  int16_t normalized_offset_slope;
  uint16_t time_constant;
  uint16_t slot;
};

struct TemperatureAcceleration {
  uint16_t k;
  uint16_t p;
  uint16_t t1;
  uint16_t t2;
};

class SEN6XComponent : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_4_0)
  SUB_SENSOR(pm_10_0)
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(voc)
  SUB_SENSOR(nox)
  SUB_SENSOR(co2)
  SUB_SENSOR(hcho)

 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  enum Sen6xType { SEN62, SEN63C, SEN65, SEN66, SEN68, SEN69C, UNKNOWN };

  void set_ambient_pressure(uint16_t ambient_pressure) { ambient_pressure_ = ambient_pressure; }
  void set_ambient_pressure_source(sensor::Sensor *pressure) { ambient_pressure_source_ = pressure; }
  void set_sensor_altitude(uint16_t sensor_altitude) { sensor_altitude_ = sensor_altitude; }
  void set_co2_automatic_self_calibration(bool enabled) { co2_asc_ = enabled; }
  void set_startup_delay(uint32_t delay_ms) { startup_delay_ms_ = delay_ms; }
  void set_auto_cleaning(bool enabled, uint32_t interval_s) {
    this->auto_cleaning_enabled_ = enabled;
    this->auto_cleaning_interval_s_ = interval_s;
  }
  bool is_measurement_running() const;
  const std::string &get_product_name() const { return this->product_name_; }
  const std::string &get_serial_number() const { return this->serial_number_; }
  uint8_t get_firmware_version_major() const { return this->firmware_version_major_; }
  uint8_t get_firmware_version_minor() const { return this->firmware_version_minor_; }
  bool get_state() const { return this->measurement_started_; }
  void set_store_baseline(bool store_baseline) { this->store_baseline_ = store_baseline; }
  void set_voc_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t std_initial, uint16_t gain_factor) {
    GasTuning tuning_params;
    tuning_params.index_offset = index_offset;
    tuning_params.learning_time_offset_hours = learning_time_offset_hours;
    tuning_params.learning_time_gain_hours = learning_time_gain_hours;
    tuning_params.gating_max_duration_minutes = gating_max_duration_minutes;
    tuning_params.std_initial = std_initial;
    tuning_params.gain_factor = gain_factor;
    this->voc_tuning_params_ = tuning_params;
  }
  void set_nox_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t gain_factor) {
    GasTuning tuning_params;
    tuning_params.index_offset = index_offset;
    tuning_params.learning_time_offset_hours = learning_time_offset_hours;
    tuning_params.learning_time_gain_hours = learning_time_gain_hours;
    tuning_params.gating_max_duration_minutes = gating_max_duration_minutes;
    tuning_params.std_initial = NOX_DEFAULT_STD_INITIAL;
    tuning_params.gain_factor = gain_factor;
    this->nox_tuning_params_ = tuning_params;
  }
  void set_temperature_compensation(float offset, float normalized_offset_slope, uint16_t time_constant,
                                    uint16_t slot) {
    TemperatureCompensation temp_comp;
    temp_comp.offset = offset * 200;
    temp_comp.normalized_offset_slope = normalized_offset_slope * 10000;
    temp_comp.time_constant = time_constant;
    temp_comp.slot = slot;
    this->temperature_compensation_ = temp_comp;
  }
  bool apply_temperature_compensation(float offset, float normalized_offset_slope, uint16_t time_constant,
                                      uint16_t slot);
  void set_temperature_acceleration(float k, float p, float t1, float t2) {
    TemperatureAcceleration temp_accel;
    temp_accel.k = k * 10;
    temp_accel.p = p * 10;
    temp_accel.t1 = t1 * 10;
    temp_accel.t2 = t2 * 10;
    this->temperature_acceleration_ = temp_accel;
  }
  void set_type(const std::string &type) { this->sen6x_type_ = infer_type_from_product_name_(type); }
  bool start_fan_cleaning();
  bool perform_forced_co2_recalibration(uint16_t reference_ppm);
  bool co2_sensor_factory_reset();
  bool reset_device();
  bool activate_sht_heater();
  bool get_sht_heater_measurements();
  bool start_measurement();
  bool stop_measurement();

 protected:
  bool update_ambient_pressure_compensation_(uint16_t pressure_hpa);
  bool write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning);
  bool write_temperature_compensation_(const TemperatureCompensation &compensation);
  bool write_temperature_acceleration_(const TemperatureAcceleration &acceleration);
  Sen6xType infer_type_from_product_name_(const std::string &product_name);
  void schedule_post_setup_commands_();
  void finish_setup_();

  ERRORCODE error_code_{COMMUNICATION_FAILED};
  bool initialized_{false};
  sensor::Sensor *ambient_pressure_source_{nullptr};
  std::string product_name_;
  Sen6xType sen6x_type_{UNKNOWN};
  std::string serial_number_;
  uint8_t firmware_version_major_{0};
  uint8_t firmware_version_minor_{0};
  Sen6xBaselines voc_baselines_storage_{};
  bool store_baseline_{false};
  uint32_t last_baseline_store_ms_{0};
  ESPPreferenceObject pref_;

  optional<GasTuning> voc_tuning_params_;
  optional<GasTuning> nox_tuning_params_;
  optional<TemperatureCompensation> temperature_compensation_;
  optional<TemperatureAcceleration> temperature_acceleration_;
  optional<uint16_t> ambient_pressure_;
  optional<uint16_t> sensor_altitude_;
  optional<bool> co2_asc_;
  optional<uint16_t> ambient_pressure_read_;
  optional<uint16_t> sensor_altitude_read_;
  optional<bool> co2_asc_read_;
  optional<bool> auto_cleaning_enabled_;
  optional<uint32_t> auto_cleaning_interval_s_;
  bool measurement_started_{false};
  uint32_t startup_delay_ms_{60000};
  uint32_t startup_stable_after_{0};
  uint32_t last_stop_ms_{0};
  uint32_t last_cleaning_ms_{0};
  bool auto_clean_restart_pending_{false};
  bool has_last_values_{false};
  float last_pm_1_0_{NAN};
  float last_pm_2_5_{NAN};
  float last_pm_4_0_{NAN};
  float last_pm_10_0_{NAN};
  float last_temperature_{NAN};
  float last_humidity_{NAN};
  float last_voc_{NAN};
  float last_nox_{NAN};
  float last_hcho_{NAN};
  float last_co2_{NAN};
};

}  // namespace esphome::sen6x
