#include "esphome/components/binary_sensor/binary_sensor.h"

class TM1638Key : public binary_sensor::BinarySensor {
  friend class TM1638Component;

 public:
  void set_keycode(uint8_t key_code) { key_code_ = key_code; }  //needed for binary sensor
  void process(uint8_t data) {

    uint8_t mask = 1;

    data = data >> key_code_;
    data = data & mask;

    this->publish_state(static_cast<bool>(data));
  }

 protected:
  uint8_t key_code_{0};
};
