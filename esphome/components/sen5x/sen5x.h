#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace sen5x {

enum ERRORCODE {
  COMMUNICATION_FAILED,
  SERIAL_NUMBER_IDENTIFICATION_FAILED,
  MEASUREMENT_INIT_FAILED,
  PRODUCT_NAME_FAILED,
  FIRMWARE_FAILED,
  UNKNOWN
};

// Shortest time interval of 3H for storing baseline values.
// Prevents wear of the flash because of too many write operations
const uint32_t SHORTEST_BASELINE_STORE_INTERVAL = 10800;
// Store anyway if the baseline difference exceeds the max storage diff value
const uint32_t MAXIMUM_STORAGE_DIFF = 50;

struct Sen5xBaselines {
  int32_t state0;
  int32_t state1;
} PACKED;  // NOLINT

enum RhtAccelerationMode : uint16_t { LOW_ACCELERATION = 0, MEDIUM_ACCELERATION = 1, HIGH_ACCELERATION = 2 };

struct GasTuning {
  uint16_t index_offset;
  uint16_t learning_time_offset_hours;
  uint16_t learning_time_gain_hours;
  uint16_t gating_max_duration_minutes;
  uint16_t std_initial;
  uint16_t gain_factor;
};

struct TemperatureCompensation {
  uint16_t offset;
  uint16_t normalized_offset_slope;
  uint16_t time_constant;
};

class SEN5XComponent : public PollingComponent, public sensirion_common::SensirionI2CDevice {
 public:
  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  enum Sen5xType { SEN50, SEN54, SEN55, UNKNOWN };

  void set_pm_1_0_sensor(sensor::Sensor *pm_1_0) { pm_1_0_sensor_ = pm_1_0; }
  void set_pm_2_5_sensor(sensor::Sensor *pm_2_5) { pm_2_5_sensor_ = pm_2_5; }
  void set_pm_4_0_sensor(sensor::Sensor *pm_4_0) { pm_4_0_sensor_ = pm_4_0; }
  void set_pm_10_0_sensor(sensor::Sensor *pm_10_0) { pm_10_0_sensor_ = pm_10_0; }

  void set_voc_sensor(sensor::Sensor *voc_sensor) { voc_sensor_ = voc_sensor; }
  void set_nox_sensor(sensor::Sensor *nox_sensor) { nox_sensor_ = nox_sensor; }
  void set_humidity_sensor(sensor::Sensor *humidity_sensor) { humidity_sensor_ = humidity_sensor; }
  void set_temperature_sensor(sensor::Sensor *temperature_sensor) { temperature_sensor_ = temperature_sensor; }
  void set_store_baseline(bool store_baseline) { store_baseline_ = store_baseline; }
  void set_acceleration_mode(RhtAccelerationMode mode) { acceleration_mode_ = mode; }
  void set_auto_cleaning_interval(uint32_t auto_cleaning_interval) { auto_cleaning_interval_ = auto_cleaning_interval; }
  void set_voc_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t std_initial, uint16_t gain_factor) {
    voc_tuning_params_.value().index_offset = index_offset;
    voc_tuning_params_.value().learning_time_offset_hours = learning_time_offset_hours;
    voc_tuning_params_.value().learning_time_gain_hours = learning_time_gain_hours;
    voc_tuning_params_.value().gating_max_duration_minutes = gating_max_duration_minutes;
    voc_tuning_params_.value().std_initial = std_initial;
    voc_tuning_params_.value().gain_factor = gain_factor;
  }
  void set_nox_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t gain_factor) {
    nox_tuning_params_.value().index_offset = index_offset;
    nox_tuning_params_.value().learning_time_offset_hours = learning_time_offset_hours;
    nox_tuning_params_.value().learning_time_gain_hours = learning_time_gain_hours;
    nox_tuning_params_.value().gating_max_duration_minutes = gating_max_duration_minutes;
    nox_tuning_params_.value().std_initial = 50;
    nox_tuning_params_.value().gain_factor = gain_factor;
  }
  void set_temperature_compensation(float offset, float normalized_offset_slope, uint16_t time_constant) {
    temperature_compensation_.value().offset = offset * 200;
    temperature_compensation_.value().normalized_offset_slope = normalized_offset_slope * 100;
    temperature_compensation_.value().time_constant = time_constant;
  }
  bool start_fan_cleaning();

 protected:
  bool write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning);
  bool write_temperature_compensation_(const TemperatureCompensation &compensation);
  ERRORCODE error_code_;
  bool initialized_{false};
  sensor::Sensor *pm_1_0_sensor_{nullptr};
  sensor::Sensor *pm_2_5_sensor_{nullptr};
  sensor::Sensor *pm_4_0_sensor_{nullptr};
  sensor::Sensor *pm_10_0_sensor_{nullptr};
  // SEN54 and SEN55 only
  sensor::Sensor *temperature_sensor_{nullptr};
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *voc_sensor_{nullptr};
  // SEN55 only
  sensor::Sensor *nox_sensor_{nullptr};

  std::string product_name_;
  uint8_t serial_number_[4];
  uint16_t firmware_version_;
  Sen5xBaselines voc_baselines_storage_;
  bool store_baseline_;
  uint32_t seconds_since_last_store_;
  ESPPreferenceObject pref_;
  optional<RhtAccelerationMode> acceleration_mode_;
  optional<uint32_t> auto_cleaning_interval_;
  optional<GasTuning> voc_tuning_params_;
  optional<GasTuning> nox_tuning_params_;
  optional<TemperatureCompensation> temperature_compensation_;
};

}  // namespace sen5x
}  // namespace esphome
