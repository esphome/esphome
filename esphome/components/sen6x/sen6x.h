#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/sensirion_common/i2c_sensirion.h"
#include <string>

namespace esphome::sen6x {

class SEN6XComponent : public PollingComponent, public sensirion_common::SensirionI2CDevice {
  // Mass Concentration Sensors
  SUB_SENSOR(pm_1_0)
  SUB_SENSOR(pm_2_5)
  SUB_SENSOR(pm_4_0)
  SUB_SENSOR(pm_10_0)
  // Number Concentration Sensors
  SUB_SENSOR(pm_nc_0_5)
  SUB_SENSOR(pm_nc_1_0)
  SUB_SENSOR(pm_nc_2_5)
  SUB_SENSOR(pm_nc_4_0)
  SUB_SENSOR(pm_nc_10_0)
  // Environment & Gas Sensors
  SUB_SENSOR(temperature)
  SUB_SENSOR(humidity)
  SUB_SENSOR(voc)
  SUB_SENSOR(nox)
  SUB_SENSOR(co2)
  SUB_SENSOR(hcho)

 public:
  enum Sen6xType { SEN62, SEN63C, SEN65, SEN66, SEN68, SEN69C, UNKNOWN };
  enum State { STARTING, WARMING_UP, MEASURING, IDLE };

  float get_setup_priority() const override { return setup_priority::DATA; }
  void setup() override;
  void dump_config() override;
  void update() override;

  // Configuration
  void set_type(const std::string &type) { this->sen6x_type_ = this->infer_type_from_product_name_(type); }

  // Measurement Control
  void stop_measurement();
  void start_continuous_measurement();
  bool get_data_ready();
  void device_reset();
  uint32_t get_device_status(bool clear_after_read = false);

  // Temperature & Compensation
  void set_temperature_offset(float offset_c);
  void set_temperature_acceleration(uint16_t profile); // 0: default, 1: slow, 2: fast
  void set_ambient_pressure(uint16_t pressure_hpa);
  uint16_t get_ambient_pressure();
  void set_sensor_altitude(uint16_t altitude_meters);
  uint16_t get_sensor_altitude();

  // Device Info Getters
  std::string get_product_name() const { return this->product_name_; }
  std::string get_serial_number() const { return this->serial_number_; }
  std::string get_firmware_version() const {
    return std::to_string(this->firmware_version_major_) + "." +
           std::to_string(this->firmware_version_minor_);
  }
  std::string get_state() const {
    switch (this->state_) {
      case STARTING: return "STARTING";
      case WARMING_UP: return "WARMING_UP";
      case MEASURING: return "MEASURING";
      case IDLE: return "IDLE";
      default: return "";
    }
  }

  // Maintenance & Advanced Features
  void start_fan_cleaning();
  void activate_sht_heater();
  bool get_sht_heater_measurements(float &temp, float &hum);

  // Tuning Parameters (VOC & NOx)
  void set_voc_algorithm_tuning_parameters(uint16_t index_offset, uint16_t learning_time, uint16_t gain, uint16_t gate_max);
  void set_nox_algorithm_tuning_parameters(uint16_t index_offset, uint16_t learning_time, uint16_t gain, uint16_t gate_max);
  bool get_voc_algorithm_tuning_parameters(uint16_t &index_offset, uint16_t &learning_time, uint16_t &gain, uint16_t &gate_max);
  bool get_nox_algorithm_tuning_parameters(uint16_t &index_offset, uint16_t &learning_time, uint16_t &gain, uint16_t &gate_max);

  // CO2 Calibration
  void set_co2_automatic_self_calibration(bool enable);
  bool get_co2_automatic_self_calibration();
  void perform_forced_co2_recalibration(uint16_t co2_ppm);
  void perform_co2_sensor_factory_reset();

 protected:
  Sen6xType sen6x_type_{UNKNOWN};
  State state_{STARTING};

  bool is_idle_() const { return this->state_ == IDLE; }
  bool is_measuring_() const { return this->state_ == WARMING_UP || this->state_ == MEASURING; }

  std::string product_name_;
  std::string serial_number_;
  uint8_t firmware_version_major_{0};
  uint8_t firmware_version_minor_{0};

  uint16_t read_cmd_{0};
  uint8_t poll_retries_remaining_{0};
  uint8_t read_words_{0};

  // Internal Logic
  Sen6xType infer_type_from_product_name_(const std::string &product_name);
  void poll_data_ready_();
  void read_measurements_();
  void parse_and_publish_measurements_();
  void read_number_concentration_();
  void parse_and_publish_number_concentration_();
};

}  // namespace esphome::sen6x
