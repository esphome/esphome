#pragma once
#include "esphome/core/component.h"

namespace esphome {
namespace beacon {
class Beacon : public Component {
  void loop() override;
  void setup() override;
};
}  // namespace beacon
}  // namespace esphome
