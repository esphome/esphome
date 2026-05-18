#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"

#include <cmath>

namespace esphome::sen6x {

// Default NOx std_initial value per Sensirion specification
static constexpr uint16_t NOX_DEFAULT_STD_INITIAL = 50;

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
  const std::string &get_product_name() const { return this->product_name_; }
  const std::string &get_serial_number() const { return this->serial_number_; }
  uint8_t get_firmware_version_major() const { return this->firmware_version_major_; }
  uint8_t get_firmware_version_minor() const { return this->firmware_version_minor_; }
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
    temp_comp.offset = static_cast<int16_t>(lroundf(offset * 200.0f));
    temp_comp.normalized_offset_slope = static_cast<int16_t>(lroundf(normalized_offset_slope * 10000.0f));
    temp_comp.time_constant = time_constant;
    temp_comp.slot = slot;
    this->temperature_compensation_ = temp_comp;
  }
  void set_temperature_acceleration(float k, float p, float t1, float t2) {
    TemperatureAcceleration temp_accel;
    temp_accel.k = static_cast<uint16_t>(lroundf(k * 10.0f));
    temp_accel.p = static_cast<uint16_t>(lroundf(p * 10.0f));
    temp_accel.t1 = static_cast<uint16_t>(lroundf(t1 * 10.0f));
    temp_accel.t2 = static_cast<uint16_t>(lroundf(t2 * 10.0f));
    this->temperature_acceleration_ = temp_accel;
  }
  void set_type(const std::string &type) { this->sen6x_type_ = infer_type_from_product_name_(type); }

 protected:
  bool update_ambient_pressure_compensation_(uint16_t pressure_hpa);
  bool write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning);
  bool write_temperature_compensation_(const TemperatureCompensation &compensation);
  bool write_temperature_acceleration_(const TemperatureAcceleration &acceleration);
  Sen6xType infer_type_from_product_name_(const std::string &product_name);
  void schedule_post_setup_commands_();
  void run_next_setup_step_();
  void finish_setup_();
  void poll_data_ready_();
  void read_measurements_();
  void parse_and_publish_measurements_();

  bool initialized_{false};
  sensor::Sensor *ambient_pressure_source_{nullptr};
  std::string product_name_;
  Sen6xType sen6x_type_{UNKNOWN};
  std::string serial_number_;
  uint16_t read_cmd_{0};
  uint8_t firmware_version_major_{0};
  uint8_t firmware_version_minor_{0};
  uint8_t poll_retries_remaining_{0};
  uint8_t read_words_{0};
  size_t setup_step_index_{0};

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
  uint32_t startup_delay_ms_{60000};
  bool startup_complete_{false};
};

}  // namespace esphome::sen6x
