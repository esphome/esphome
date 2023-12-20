#pragma once

#include "esphome/core/component.h"
#include "esphome/core/application.h"

#include <vector>

namespace esphome {
namespace mbus {

class MBus : public Component {
 public:
  MBus() = default;

  void setup() override;

  void loop() override;

  void dump_config() override;

  float get_setup_priority() const override;

 protected:
};

}  // namespace mbus
}  // namespace esphome
