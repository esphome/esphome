#pragma once

#include "esphome/components/sx1509/sx1509.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace sx1509 {

class SX1509BinarySensor : public sx1509::SX1509Processor, public binary_sensor::BinarySensor {
 public:
  void set_row_col(uint8_t row, uint8_t col) { this->key_ = (1 << (col + 8)) | (1 << row); }
  void process(uint16_t data) override { this->publish_state(static_cast<bool>(data == key_)); }

 protected:
  uint16_t key_{0};
};

}  // namespace sx1509
}  // namespace esphome
