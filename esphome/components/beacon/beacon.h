#pragma once
#include "esphome/core/component.h"

namespace esphome {
namespace ble {
class Beacon : public Component {
  void loop() override;
  void setup() override;
};
}  // namespace ble
}  // namespace esphome
