#pragma once

#include "sx1509.h"
#include "esphome/components/output/float_output.h"
#include "esphome/components/sensor/sensor.h"

namespace esphome {
namespace sx1509 {

class SX1509Component;

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

 protected:
  SX1509Component *parent_;
  uint8_t **keypad_values_;
  uint8_t rows_ = {};
  uint8_t cols_ = {};
  uint16_t sleep_time_ = {};
  uint8_t scan_time_ = {};
  uint8_t debounce_time_ = {};
  uint16_t last_key_press_ = {0};

  uint8_t get_row_(uint16_t key_data);
  uint8_t get_col_(uint16_t key_data);
};

}  // namespace sx1509
}  // namespace esphome