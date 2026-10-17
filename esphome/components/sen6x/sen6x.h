#pragma once

#include "esphome/core/component.h"
#include "esphome/core/optional.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"
#ifdef SEN6X_STATUS_SENSOR_COUNT
#include "esphome/components/binary_sensor/binary_sensor.h"
#include "esphome/core/helpers.h"
#endif

namespace esphome::sen6x {

#ifdef SEN6X_STATUS_SENSOR_COUNT
// Bits of the device status register (datasheet section 4.3) a binary sensor can watch
enum class Sen6xStatus : uint8_t {
  SEN6X_STATUS_FAN_ERROR,
  SEN6X_STATUS_FAN_SPEED_WARNING,
  SEN6X_STATUS_RHT_ERROR,
  SEN6X_STATUS_GAS_ERROR,
  SEN6X_STATUS_CO2_ERROR,
  SEN6X_STATUS_HCHO_ERROR,
  SEN6X_STATUS_PM_ERROR,
};

struct StatusSensor {
  binary_sensor::BinarySensor *sensor;
  Sen6xStatus status;
};
#endif

// The NOx algorithm requires std_initial to stay at 50 (Sensirion datasheet)
static constexpr uint16_t NOX_STD_INITIAL = 50;

// Raw parameter block for the VOC/NOx algorithm tuning commands
struct GasTuning {
  uint16_t index_offset;
  uint16_t learning_time_offset_hours;
  uint16_t learning_time_gain_hours;
  uint16_t gating_max_duration_minutes;
  uint16_t std_initial;
  uint16_t gain_factor;
};

// Raw values in the device's fixed-point encoding (offset x200, slope x10000)
struct TemperatureCompensation {
  int16_t offset;
  int16_t normalized_offset_slope;
  uint16_t time_constant;
};

// Raw values in the device's fixed-point encoding (all x10)
struct TemperatureAcceleration {
  uint16_t k;
  uint16_t p;
  uint16_t t1;
  uint16_t t2;
};

// The CO2 settings a config may leave out: altitude is 0 to 3000 m, pressure 700 to 1200 hPa
inline constexpr uint16_t ALTITUDE_UNSET = 0xFFFF;
inline constexpr uint16_t PRESSURE_UNSET = 0;

class SEN6XComponent final : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_4_0)
  SUB_SENSOR(pm_10_0)
#ifdef USE_SEN6X_NUMBER_CONCENTRATION
  SUB_SENSOR(pmc_0_5)
  SUB_SENSOR(pmc_1_0)
  SUB_SENSOR(pmc_2_5)
  SUB_SENSOR(pmc_4_0)
  SUB_SENSOR(pmc_10_0)
#endif
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

  void set_type(const std::string &type) { sen6x_type_ = infer_type_from_product_name_(type); }
  void set_voc_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t std_initial, uint16_t gain_factor) {
    this->voc_tuning_params_ = GasTuning{
        index_offset, learning_time_offset_hours, learning_time_gain_hours, gating_max_duration_minutes, std_initial,
        gain_factor};
  }
  void set_nox_algorithm_tuning(uint16_t index_offset, uint16_t learning_time_offset_hours,
                                uint16_t learning_time_gain_hours, uint16_t gating_max_duration_minutes,
                                uint16_t gain_factor) {
    this->nox_tuning_params_ = GasTuning{index_offset,
                                         learning_time_offset_hours,
                                         learning_time_gain_hours,
                                         gating_max_duration_minutes,
                                         NOX_STD_INITIAL,
                                         gain_factor};
  }
  void set_startup_delay(uint32_t delay_ms) { this->startup_delay_ms_ = delay_ms; }
  void set_temperature_compensation(int16_t offset, int16_t normalized_offset_slope, uint16_t time_constant) {
    this->temperature_compensation_ = TemperatureCompensation{offset, normalized_offset_slope, time_constant};
  }
  void set_temperature_acceleration(uint16_t k, uint16_t p, uint16_t t1, uint16_t t2) {
    this->temperature_acceleration_ = TemperatureAcceleration{k, p, t1, t2};
  }
#ifdef SEN6X_STATUS_SENSOR_COUNT
  void add_status_binary_sensor(binary_sensor::BinarySensor *sensor, Sen6xStatus status) {
    this->status_sensors_.push_back({sensor, status});
  }
#endif
  void set_automatic_self_calibration(bool enabled) { this->co2_asc_ = enabled; }
  void set_altitude_compensation(uint16_t altitude) { this->altitude_compensation_ = altitude; }
  void set_ambient_pressure_compensation(uint16_t pressure_hpa) { this->ambient_pressure_ = pressure_hpa; }
  void set_ambient_pressure_source(sensor::Sensor *pressure) { this->ambient_pressure_source_ = pressure; }
  void start_measurement();
  void stop_measurement();
  void start_fan_cleaning();
  void activate_sht_heater();

 protected:
  enum SetupStep : uint8_t {
    SETUP_STEP_VOC_TUNING,
    SETUP_STEP_NOX_TUNING,
    SETUP_STEP_TEMPERATURE_COMPENSATION,
    SETUP_STEP_TEMPERATURE_ACCELERATION,
    SETUP_STEP_CO2_ASC,
    SETUP_STEP_ALTITUDE,
    SETUP_STEP_AMBIENT_PRESSURE,
    SETUP_STEP_COUNT,
  };

  Sen6xType infer_type_from_product_name_(const std::string &product_name);
  void run_next_setup_step_();
  bool write_setup_step_(SetupStep step);
  void finish_setup_();
  bool write_config_words_(uint16_t i2c_command, const uint16_t *data, uint8_t len);
  bool write_tuning_parameters_(uint16_t i2c_command, const GasTuning &tuning);
  bool write_setup_register_(uint16_t i2c_command, uint16_t value);
  bool write_temperature_compensation_(const TemperatureCompensation &compensation);
  bool write_temperature_acceleration_(const TemperatureAcceleration &acceleration);
  void write_ambient_pressure_(uint16_t pressure_hpa);
  void update_ambient_pressure_compensation_(float pressure_hpa);
  uint32_t command_wait_remaining_() const;
  bool command_blocked_() const;
  void set_command_wait_(uint32_t wait_ms);
  void on_measurement_started_();
  void poll_data_ready_();
  void read_measurements_();
  void parse_and_publish_measurements_();
  void read_device_status_();
#ifdef SEN6X_STATUS_SENSOR_COUNT
  void parse_and_publish_device_status_();
#endif
#ifdef USE_SEN6X_NUMBER_CONCENTRATION
  bool has_number_concentration_sensor_() const;
  void read_number_concentration_();
  void parse_and_publish_number_concentration_();
#endif

  std::string product_name_;
  std::string serial_number_;
#ifdef SEN6X_STATUS_SENSOR_COUNT
  StaticVector<StatusSensor, SEN6X_STATUS_SENSOR_COUNT> status_sensors_;
#endif
  optional<GasTuning> voc_tuning_params_;
  optional<GasTuning> nox_tuning_params_;
  optional<TemperatureAcceleration> temperature_acceleration_;
  optional<TemperatureCompensation> temperature_compensation_;
  uint16_t read_cmd_{0};
  sensor::Sensor *ambient_pressure_source_{nullptr};
  uint32_t startup_delay_ms_{60000};
  Sen6xType sen6x_type_{UNKNOWN};
  // Post-command wait windows, stored as start time plus duration so elapsed-time checks survive millis() wrap
  uint32_t command_started_at_{0};
  uint32_t command_wait_{0};
  uint32_t co2_started_at_{0};
  uint32_t co2_wait_{0};
  uint16_t altitude_compensation_{ALTITUDE_UNSET};
  uint16_t ambient_pressure_{PRESSURE_UNSET};
  // Last pressure written to the device, used to skip redundant writes
  uint16_t last_ambient_pressure_{PRESSURE_UNSET};
  optional<bool> co2_asc_;
  uint8_t setup_step_index_{0};
  uint8_t firmware_version_major_{0};
  uint8_t firmware_version_minor_{0};
  uint8_t poll_retries_remaining_{0};
  uint8_t read_words_{0};
  bool initialized_{false};
  bool pressure_range_warned_{false};
  bool measuring_{false};
  bool startup_complete_{false};
};

}  // namespace esphome::sen6x
