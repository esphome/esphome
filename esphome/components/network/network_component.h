#pragma once
#ifdef USE_ESP32
#include "esphome/core/component.h"

namespace esphome::network {

/// Component that initializes the network stack early.
/// This allows web_server to bind before WiFi/Ethernet setup.
class NetworkComponent : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override;
};

}  // namespace esphome::network
#endif
