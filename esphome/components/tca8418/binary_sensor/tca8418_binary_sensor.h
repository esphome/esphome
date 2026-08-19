#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../tca8418.h"

namespace esphome::tca8418 {

/// Reports a single key of a TCA8418. The key is identified either by its
/// position in the matrix, by the character it is mapped to, or by the key
/// number reported by the device. Exactly one of those is configured; the
/// others keep a value that never matches.
class TCA8418BinarySensor : public binary_sensor::BinarySensor, public TCA8418Listener {
 public:
  void set_position(uint8_t row, uint8_t col) {
    this->row_ = row;
    this->col_ = col;
  }
  void set_key(uint8_t key) { this->key_ = key; }
  void set_key_char(uint8_t key_char) { this->key_char_ = key_char; }

  void button_pressed(uint8_t row, uint8_t col) override {
    if (row == this->row_ && col == this->col_)
      this->publish_state(true);
  }
  void button_released(uint8_t row, uint8_t col) override {
    if (row == this->row_ && col == this->col_)
      this->publish_state(false);
  }
  void key_pressed(uint8_t key) override {
    if (key == this->key_)
      this->publish_state(true);
  }
  void key_released(uint8_t key) override {
    if (key == this->key_)
      this->publish_state(false);
  }
  void key_char_pressed(uint8_t key_char) override {
    if (key_char == this->key_char_)
      this->publish_state(true);
  }
  void key_char_released(uint8_t key_char) override {
    if (key_char == this->key_char_)
      this->publish_state(false);
  }

 protected:
  uint8_t row_{0xFF};
  uint8_t col_{0xFF};
  uint8_t key_{0};
  uint8_t key_char_{0};
};

}  // namespace esphome::tca8418
