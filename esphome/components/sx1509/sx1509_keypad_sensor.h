#pragma once

#include "sx1509.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace sx1509 {

class SX1509Component;

class SX1509BinarySensor : public binary_sensor::BinarySensor {
  friend class SX1509KeypadSensor;

 public:
  void set_row_col(uint8_t row, uint8_t col) { this->key_ = (1 << (col + 8)) | (1 << row); }
  void process(uint16_t keydata) { this->publish_state(static_cast<bool>(keydata == key_)); }

 protected:
  uint16_t key_{0};
};

class SX1509KeypadSensor : public sensor::Sensor, public Component {
 public:
  void set_parent(SX1509Component *parent) { this->parent_ = parent; }
  void set_rows_cols(uint8_t rows, uint8_t cols) {
    this->rows_ = rows;
    this->cols_ = cols;
  };
  void set_timers(uint16_t sleep_time, uint8_t scan_time, uint8_t debounce_time) {
    this->sleep_time_ = sleep_time;
    this->scan_time_ = scan_time;
    this->debounce_time_ = debounce_time;
  };
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE; }
  void loop() override;
  void register_binary_sensor(SX1509BinarySensor *binary_sensor) { this->binary_sensors_.push_back(binary_sensor); };

 protected:
  SX1509Component *parent_;
  uint8_t rows_ = {};
  uint8_t cols_ = {};
  uint16_t sleep_time_ = {};
  uint8_t scan_time_ = {};
  uint8_t debounce_time_ = {};
  uint16_t last_key_press_ = {0};
  std::vector<SX1509BinarySensor *> binary_sensors_{};

  uint8_t get_row_(uint16_t key_data);
  uint8_t get_col_(uint16_t key_data);
};

}  // namespace sx1509
}  // namespace esphome
