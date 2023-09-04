#pragma once

#include <vector>
#include <cstdint>
#include <functional>
#include "esphome/components/keyboard/keyboard.h"

namespace esphome {
namespace hid {

class MediaKeys : public keyboard::KeyboardControl {
 public:
  void control(std::vector<uint16_t> &&keys) override;

 protected:
  virtual void report() = 0;
  uint16_t media_keys_;
};

}  // namespace hid
}  // namespace esphome
