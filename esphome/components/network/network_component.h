#pragma once
#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && defined(USE_ESP32)
#include "esphome/core/component.h"

#ifdef USE_NETWORK_DEFAULT_ROUTE
#include "esphome/core/helpers.h"
// Forward declaration matching esp_netif's own typedef; avoids pulling esp_netif.h
// into this header.
using esp_netif_t = struct esp_netif_obj;
#endif

namespace esphome::network {

#ifdef USE_NETWORK_DEFAULT_ROUTE
/// The interface holding the default route. Lists only the interfaces loop() arbitrates
/// (ARBITRATED_NETWORK_TYPES in __init__.py); extend both together.
enum class NetworkInterface : uint8_t {
  NETWORK_INTERFACE_NONE,
  NETWORK_INTERFACE_ETHERNET,
  NETWORK_INTERFACE_WIFI,
};

#ifdef NETWORK_ACTIVE_INTERFACE_LISTENER_COUNT
/// Notified from the main loop when the interface holding the default route changes.
class ActiveInterfaceListener {
 public:
  virtual void on_active_interface(NetworkInterface interface) = 0;
};
#endif
#endif

class NetworkComponent final : public Component {
 public:
  void setup() override;
  void dump_config() override;
  // AFTER_BLUETOOTH: BLE controller must initialize before esp_netif_init per IDF guidance.
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

#ifdef USE_NETWORK_DEFAULT_ROUTE
  void loop() override;

  /// The first connected interface in network: priority order; NONE while none is connected.
  NetworkInterface get_active_interface() const { return this->active_interface_; }
#ifdef NETWORK_ACTIVE_INTERFACE_LISTENER_COUNT
  SLOT_LIST(ActiveInterfaceListener, active_interface_listeners_, add_active_interface_listener,
            NETWORK_ACTIVE_INTERFACE_LISTENER_COUNT)
#endif

 protected:
  // Verify-lwIP-route interval for the settled state; keeps the global lwIP core
  // mutex off the hot loop path.
  static constexpr uint32_t ROUTE_CHECK_INTERVAL_MS = 1000;
  // Last netif this component made the default; avoids redundant esp_netif calls.
  esp_netif_t *default_netif_{nullptr};
  uint32_t last_route_check_{0};
  NetworkInterface active_interface_{NetworkInterface::NETWORK_INTERFACE_NONE};
#endif
};
}  // namespace esphome::network
#endif
