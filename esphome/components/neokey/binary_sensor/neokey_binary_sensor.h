#pragma once

#include "../neokey.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

namespace esphome {
namespace neokey {

class NeoKeyBinarySensor : public binary_sensor::BinarySensor, public KeyListener {
 public:
  void set_key(uint8_t key) { key_ = key; };
  void keys_update(uint8_t keys) override;
 protected:
  uint8_t key_{0};
};

}  // namespace neokey
}  // namespace esphome
