#pragma once
#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && defined(USE_ESP32)
#include "esphome/core/component.h"

#ifdef USE_NETWORK_DEFAULT_ROUTE
// Forward declaration matching esp_netif's own typedef; avoids pulling esp_netif.h
// into this header.
using esp_netif_t = struct esp_netif_obj;
#endif

namespace esphome::network {
class NetworkComponent final : public Component {
 public:
  void setup() override;
  // AFTER_BLUETOOTH: BLE controller must initialize before esp_netif_init per IDF guidance.
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

#ifdef USE_NETWORK_DEFAULT_ROUTE
  void loop() override;

 protected:
  // Last netif this component made the default; avoids redundant esp_netif calls.
  esp_netif_t *default_netif_{nullptr};
#endif
};
}  // namespace esphome::network
#endif
