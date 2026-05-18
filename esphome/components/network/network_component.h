#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/component.h"

namespace esphome::network {
class NetworkComponent : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }
};
}  // namespace esphome::network
#endif
