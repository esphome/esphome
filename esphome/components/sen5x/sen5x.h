#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace sen5x {

enum ERRORCODE : uint8_t {
  COMMUNICATION_FAILED,
  SERIAL_NUMBER_IDENTIFICATION_FAILED,
  MEASUREMENT_INIT_FAILED,
  PRODUCT_NAME_FAILED,
  FIRMWARE_FAILED,
  UNSUPPORTED_CONF,
  UNKNOWN
};

enum Sen5xType { SEN50, SEN54, SEN55, SEN60, SEN63C, SEN65, SEN66, SEN68, UNKNOWN_MODEL };
enum RhtAccelerationMode : uint16_t { LOW_ACCELERATION = 0, MEDIUM_ACCELERATION = 1, HIGH_ACCELERATION = 2 };
enum SetupStates {
  SM_START,
  SM_START_1,
  SM_START_2,
  SM_GET_SN,
  SM_GET_PN,
  SM_GET_FW,
  SM_SET_VOCB,
  SM_SET_ACI,
  SM_SET_RHTAM,
  SM_SET_VOCT,
  SM_SET_NOXT,
  SM_SET_TP,
  SM_SET_CO2ASC,
  SM_SET_CO2AC,
  SM_SENSOR_CHECK,
  SM_START_MEAS,
  SM_DONE
};

struct Sen5xBaselines {
  int32_t state0;
  int32_t state1;
} PACKED;  // NOLINT

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
};

// Shortest time interval of 3H for storing baseline values.
// Prevents wear of the flash because of too many write operations
static const uint32_t SHORTEST_BASELINE_STORE_INTERVAL = 10800;
// Store anyway if the baseline difference exceeds the max storage diff value
static const uint32_t MAXIMUM_STORAGE_DIFF = 50;

class SEN5XComponent : public PollingComponent, public sensirion_common::SensirionI2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  void update() override;

  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0) { this->pm_1_0_sensor_ = pm_1_0; }
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5) { this->pm_2_5_sensor_ = pm_2_5; }
  void set_pm_4_0_sensor(sensor::Sensor *pm_4_0) { this->pm_4_0_sensor_ = pm_4_0; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0) { this->pm_10_0_sensor_ = pm_10_0; }

  void set_voc_sensor(sensor::Sensor *voc_sensor) { this->voc_sensor_ = voc_sensor; }
  void set_nox_sensor(sensor::Sensor *nox_sensor) { this->nox_sensor_ = nox_sensor; }
  void set_co2_sensor(sensor::Sensor *co2_sensor) { this->co2_sensor_ = co2_sensor; }
  void set_hcho_sensor(sensor::Sensor *hcho_sensor) { this->hcho_sensor_ = hcho_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { this->humidity_sensor_ = humidity_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { this->temperature_sensor_ = temperature_sensor; }
  void set_store_baseline(bool store_baseline) { this->store_baseline_ = store_baseline; }
  void set_model(Sen5xType model) { this->model_ = model; }
  void set_acceleration_mode(RhtAccelerationMode mode) { this->acceleration_mode_ = mode; }
  void set_auto_cleaning_interval(uint32_t auto_cleaning_interval) {
    this->auto_cleaning_interval_ = auto_cleaning_interval;
  }
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
    tuning_params.std_initial = 50;
    tuning_params.gain_factor = gain_factor;
    this->nox_tuning_params_ = tuning_params;
  }
  void set_temperature_compensation(float offset, float normalized_offset_slope, uint16_t time_constant) {
    TemperatureCompensation temp_comp;
    temp_comp.offset = offset * 200;
    temp_comp.normalized_offset_slope = normalized_offset_slope * 10000;
    temp_comp.time_constant = time_constant;
    this->temperature_compensation_ = temp_comp;
  }
  void set_co2_auto_calibrate(bool value) { this->co2_auto_calibrate_ = value; }
  void set_co2_altitude_compensation(uint16_t altitude) { this->co2_altitude_compensation_ = altitude; }
  void set_ambient_pressure_source(sensor::Sensor *pressure) { this->co2_ambient_pressure_source_ = pressure; }
  bool start_fan_cleaning();
  bool activate_heater();
  bool perform_forced_co2_calibration(uint16_t co2);
  bool set_ambient_pressure_compensation(float pressure_in_hpa);

 protected:
  bool is_sen6x_();
  void internal_setup_(SetupStates state);
  bool start_measurements_();
  bool stop_measurements_();
  bool write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning);
  bool write_temperature_compensation_(const TemperatureCompensation &compensation);
  bool update_co2_ambient_pressure_compensation_(uint16_t pressure_in_hpa);

  uint32_t seconds_since_last_store_;
  uint16_t co2_ambient_pressure_{0};
  ERRORCODE error_code_;
  uint8_t firmware_major_{0xFF};
  uint8_t firmware_minor_{0xFF};
  bool initialized_{false};
  bool running_{false};
  bool store_baseline_;

  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_4_0_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *voc_sensor_{nullptr};
  sensor::Sensor *hcho_sensor_{nullptr};
  sensor::Sensor *nox_sensor_{nullptr};
  sensor::Sensor *co2_sensor_{nullptr};
  sensor::Sensor *co2_ambient_pressure_source_{nullptr};

  optional<Sen5xType> model_;
  optional<RhtAccelerationMode> acceleration_mode_;
  optional<uint32_t> auto_cleaning_interval_;
  optional<GasTuning> voc_tuning_params_;
  optional<GasTuning> nox_tuning_params_;
  optional<TemperatureCompensation> temperature_compensation_;
  optional<bool> co2_auto_calibrate_;
  optional<uint16_t> co2_altitude_compensation_;

  ESPPreferenceObject pref_;
  std::string product_name_ = "Unknown";
  std::string serial_number_ = "Unknown";
  Sen5xBaselines voc_baselines_storage_;
};

}  // namespace sen5x
}  // namespace esphome
