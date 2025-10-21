#pragma once

#include "esphome/components/matrix_keypad/matrix_keypad.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace matrix_keypad {

class MatrixKeypadBinarySensor : public MatrixKeypadListener, public binary_sensor::BinarySensorInitiallyOff {
 public:
  MatrixKeypadBinarySensor(uint8_t key) : has_key_(true), key_(key){};
  MatrixKeypadBinarySensor(const char *key) : has_key_(true), key_((uint8_t) key[0]){};
  MatrixKeypadBinarySensor(int row, int col) : has_key_(false), row_(row), col_(col){};

  void key_pressed(uint8_t key) override {
    if (!this->has_key_)
      return;
    if (key == this->key_)
      this->publish_state(true);
  }

  void key_released(uint8_t key) override {
    if (!this->has_key_)
      return;
    if (key == this->key_)
      this->publish_state(false);
  }

  void button_pressed(int row, int col) override {
    if (this->has_key_)
      return;
    if ((row == this->row_) && (col == this->col_))
      this->publish_state(true);
  }

  void button_released(int row, int col) override {
    if (this->has_key_)
      return;
    if ((row == this->row_) && (col == this->col_))
      this->publish_state(false);
  }

 protected:
  bool has_key_;
  uint8_t key_;
  int row_;
  int col_;
};

}  // namespace matrix_keypad
}  // namespace esphome
