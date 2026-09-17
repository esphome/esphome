#pragma once

#include "esphome/components/binary_sensor/binary_sensor.h"
#include "../tca8418.h"

namespace esphome::tca8418 {

/// Reports a single key of a TCA8418. The key can be given in the configuration
/// as a position in the matrix, as the character it is mapped to, or as the key
/// number reported by the device; all three become a key number during code
/// generation. A key starts off rather than unknown, since a key that has not
/// been reported is not being pressed.
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
