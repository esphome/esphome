#pragma once

#include "esphome/core/component.h"

namespace esphome {
namespace telnet {

class TelnetComponent : public Component {
 public:
  void loop() override;
  void setup() override;
};

}  // namespace telnet
}  // namespace esphome
