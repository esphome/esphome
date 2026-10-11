#pragma once

#include "esphome/components/number/number.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/component.h"

#include <vector>
#include <string>

namespace esphome::bq76972 {

class BQ76972AddressNumber;

class BQ76972Component final : public PollingComponent, public i2c::I2CDevice {
 public:
  void update() override;
  void setup() override;
  void dump_config() override;

  // HARDWARE_LATE setup priority
  void set_crc_mode(bool crc_mode) { this->crc_mode_ = crc_mode; }
  void set_reg_disable(bool reg_disable) { this->reg_disable_ = reg_disable; }
  void set_component_id(const std::string &id) { this->component_id_ = id; }
  void set_address(uint8_t i2c_address) { this->i2c_address_ = i2c_address; }

  // Custom I²C routines
  bool wait_for_cfgupdate();
  bool wait_for_subcommand();
  uint8_t compute_crc8(const uint8_t *data, size_t len);
  bool read_block(uint8_t start_register, uint8_t *data, size_t len);
  bool write_block(uint8_t start_register, const uint8_t *data, size_t len);
  bool write_subcommand(uint16_t subcommand, const uint8_t *data, size_t len);
  bool read_subcommand(uint16_t subcommand, uint8_t *data, size_t expected_len);
  bool bq76972_read_multi_16_le(uint8_t a_register, uint16_t *data, size_t num_words);

  // I2C reprogramming
  void set_address_number(BQ76972AddressNumber *num) { this->address_number_ = num; }
  void program_address_from_number();

  // Device related stuff
  bool store_int_temp();

  // Temperature Sensor Setters
  void publish_temperature_from_buffer(uint16_t temperature_uint16t, sensor::Sensor *sens);
  void set_internal_temp_sensor(sensor::Sensor *s) { this->internal_temp_sensor_ = s; }
  void set_cfetoff_temp_sensor(sensor::Sensor *s) { this->cfetoff_temp_sensor_ = s; }
  void set_dfetoff_temp_sensor(sensor::Sensor *s) { this->dfetoff_temp_sensor_ = s; }
  void set_alert_temp_sensor(sensor::Sensor *s) { this->alert_temp_sensor_ = s; }
  void set_ts1_temp_sensor(sensor::Sensor *s) { this->ts1_temp_sensor_ = s; }
  void set_ts2_temp_sensor(sensor::Sensor *s) { this->ts2_temp_sensor_ = s; }
  void set_ts3_temp_sensor(sensor::Sensor *s) { this->ts3_temp_sensor_ = s; }
  void set_hdq_temp_sensor(sensor::Sensor *s) { this->hdq_temp_sensor_ = s; }
  void set_dchg_temp_sensor(sensor::Sensor *s) { this->dchg_temp_sensor_ = s; }
  void set_ddsg_temp_sensor(sensor::Sensor *s) { this->ddsg_temp_sensor_ = s; }

  // Other setters
  void set_stack_voltage_sensor(sensor::Sensor *s) { this->stack_voltage_sensor_ = s; }
  void set_cell_sensor(size_t cell_idx, sensor::Sensor *s) {
    if (cell_idx < 16) {
      this->cell_sensors_[cell_idx] = s;
    }
  }

 protected:
  std::string component_id_;
  uint8_t i2c_address_;
  bool reg_disable_;
  bool crc_mode_;

  // local raw data copies
  uint16_t cell_voltages_mv_[16];
  uint16_t pack_voltage_mv_;
  float internal_temp_f_;
  float internal_temp_c_;

  // Temperature sensors
  sensor::Sensor *internal_temp_sensor_{nullptr};
  sensor::Sensor *cfetoff_temp_sensor_{nullptr};
  sensor::Sensor *dfetoff_temp_sensor_{nullptr};
  sensor::Sensor *alert_temp_sensor_{nullptr};
  sensor::Sensor *ts1_temp_sensor_{nullptr};
  sensor::Sensor *ts2_temp_sensor_{nullptr};
  sensor::Sensor *ts3_temp_sensor_{nullptr};
  sensor::Sensor *hdq_temp_sensor_{nullptr};
  sensor::Sensor *dchg_temp_sensor_{nullptr};
  sensor::Sensor *ddsg_temp_sensor_{nullptr};

  // sensor stuff
  BQ76972AddressNumber *address_number_{nullptr};
  sensor::Sensor *stack_voltage_sensor_{nullptr};
  std::vector<sensor::Sensor *> cell_sensors_{std::vector<sensor::Sensor *>(16, nullptr)};
};

class BQ76972AddressNumber final : public number::Number {
 public:
  void set_hub(BQ76972Component *hub) { this->hub_ = hub; }

 protected:
  void control(float value) override { this->publish_state(value); }
  BQ76972Component *hub_{nullptr};
};

}  // namespace esphome::bq76972
