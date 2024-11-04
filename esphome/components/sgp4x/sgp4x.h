#pragma once

#include <cinttypes>
#include <cmath>

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"
#include "esphome/core/application.h"
#include "esphome/core/preferences.h"
#include <VOCGasIndexAlgorithm.h>
#include <NOxGasIndexAlgorithm.h>

namespace esphome {
namespace sgp4x {

struct SGP4xBaselines {
  int32_t state0;
  int32_t state1;
} PACKED;  // NOLINT

enum SgpType { SGP40, SGP41 };

struct GasTuning {
  uint16_t index_offset;
  uint16_t learning_time_offset_hours;
  uint16_t learning_time_gain_hours;
  uint16_t gating_max_duration_minutes;
  uint16_t std_initial;
  uint16_t gain_factor;
};

// commands and constants
static const uint8_t SGP40_FEATURESET = 0x0020;  // can measure VOC
static const uint8_t SGP41_FEATURESET = 0x0040;  // can measure VOC and NOX
// Commands
static const uint16_t SGP4X_CMD_GET_SERIAL_ID = 0x3682;
static const uint16_t SGP4X_CMD_GET_FEATURESET = 0x202f;
static const uint16_t SGP4X_CMD_SELF_TEST = 0x280e;
static const uint16_t SGP40_CMD_MEASURE_RAW = 0x260F;
static const uint16_t SGP41_CMD_MEASURE_RAW = 0x2619;
static const uint16_t SGP41_CMD_NOX_CONDITIONING = 0x2612;
static const uint8_t SGP41_SUBCMD_NOX_CONDITIONING = 0x12;

// Shortest time interval of 3H for storing baseline values.
// Prevents wear of the flash because of too many write operations
const uint32_t SHORTEST_BASELINE_STORE_INTERVAL = 10800;
static const uint16_t SPG40_SELFTEST_TIME = 250;  // 250 ms for self test
static const uint16_t SPG41_SELFTEST_TIME = 320;  // 320 ms for self test
static const uint16_t SGP40_MEASURE_TIME = 30;
static const uint16_t SGP41_MEASURE_TIME = 55;
// Store anyway if the baseline difference exceeds the max storage diff value
const float MAXIMUM_STORAGE_DIFF = 50.0f;

class SGP4xComponent;

/// This class implements support for the Sensirion sgp4x i2c GAS (VOC) sensors.
class SGP4xComponent : public PollingComponent, public sensor::Sensor, public sensirion_common::SensirionI2CDevice {
  enum ErrorCode {
    COMMUNICATION_FAILED,
    MEASUREMENT_INIT_FAILED,
    INVALID_ID,
    UNSUPPORTED_ID,
    SERIAL_NUMBER_IDENTIFICATION_FAILED,
    SELF_TEST_FAILED,
    UNKNOWN
  } error_code_{UNKNOWN};

 public:
  // SGP4xComponent()  {};
  void set_humidity_sensor(sensor::Sensor *humidity) { humidity_sensor_ = humidity; }
  void set_temperature_sensor(sensor::Sensor *temperature) { temperature_sensor_ = temperature; }

  void setup() override;
  void update() override;
  void take_sample();
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::DATA; }
  void set_store_baseline(bool store_baseline) { store_baseline_ = store_baseline; }
  void set_voc_sensor(sensor::Sensor *voc_sensor) { voc_sensor_ = voc_sensor; }
  void set_nox_sensor(sensor::Sensor *nox_sensor) { nox_sensor_ = nox_sensor; }
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

 protected:
  void self_test_();

  /// Input sensor for humidity and temperature compensation.
  sensor::Sensor *humidity_sensor_{nullptr};
  sensor::Sensor *temperature_sensor_{nullptr};
  int16_t sensirion_init_sensors_();

  void update_gas_indices_();
  void measure_raw_();
  uint16_t voc_sraw_;
  uint16_t nox_sraw_;

  SgpType sgp_type_{SGP40};
  uint64_t serial_number_;
  uint16_t featureset_;

  bool self_test_complete_;
  uint16_t self_test_time_;

  sensor::Sensor *voc_sensor_{nullptr};
  VOCGasIndexAlgorithm voc_algorithm_;
  optional<GasTuning> voc_tuning_params_;
  float voc_state0_;
  float voc_state1_;
  int32_t voc_index_ = 0;

  sensor::Sensor *nox_sensor_{nullptr};
  int32_t nox_index_ = 0;
  NOxGasIndexAlgorithm nox_algorithm_;
  optional<GasTuning> nox_tuning_params_;

  uint16_t measure_time_;
  uint8_t samples_read_ = 0;
  uint8_t samples_to_stabilize_ = static_cast<int8_t>(GasIndexAlgorithm_INITIAL_BLACKOUT) * 2;

  bool store_baseline_;
  ESPPreferenceObject pref_;
  uint32_t seconds_since_last_store_;
  SGP4xBaselines voc_baselines_storage_;
};
}  // namespace sgp4x
}  // namespace esphome
