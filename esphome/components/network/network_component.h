#pragma once
#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/component.h"

#ifdef USE_ESP32
#include "esp_netif.h"
#include "esp_event.h"
#endif
namespace esphome {
namespace network {
class NetworkComponent : public Component {
 public:
  void setup() override;
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }
};
}  // namespace network
}  // namespace esphome
#endif
