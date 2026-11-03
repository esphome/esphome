#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../tca8418.h"

namespace esphome::tca8418 {

/// One key of a TCA8418, matched by the key number codegen resolved from the config
class TCA8418BinarySensor : public binary_sensor::BinarySensorInitiallyOff, public TCA8418Listener {
 public:
  explicit TCA8418BinarySensor(uint8_t key) : key_(key) {}

  void key_pressed(uint8_t key) override {
    if (key == this->key_)
      this->publish_state(true);
  }
  void key_released(uint8_t key) override {
    if (key == this->key_)
      this->publish_state(false);
  }

 protected:
  const uint8_t key_;
};

}  // namespace esphome::tca8418
