#pragma once
#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && defined(USE_ESP32)
#include "esphome/core/component.h"

namespace esphome::network {
class NetworkComponent : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }
};
}  // namespace esphome::network
#endif
