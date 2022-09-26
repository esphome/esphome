#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../tm1638.h"

namespace esphome {
namespace tm1638 {

class TM1638Key : public binary_sensor::BinarySensor, public KeyListener {
 public:
  void set_keycode(uint8_t key_code) { key_code_ = key_code; };
  void keys_update(uint8_t keys) override;

 protected:
  uint8_t key_code_{0};
};

}  // namespace tm1638
}  // namespace esphome
