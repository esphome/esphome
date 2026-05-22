#pragma once
#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && defined(USE_ESP32)
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"

#include "esp_event.h"
#include "esp_netif.h"

namespace esphome::network {

// Cap matches the number of interface types the priority list accepts in YAML
// (ethernet, wifi, openthread, modem). StaticVector keeps zero heap allocation.
inline constexpr size_t MAX_NETWORK_PRIORITY_ENTRIES = 4;

struct NetworkPriorityEntry {
  const char *interface;  // YAML name: "ethernet", "wifi", "openthread", "modem"
  uint32_t timeout_ms;    // 0 = no timeout; consumed by Unit D (runtime fallback)
};

class NetworkComponent : public Component {
 public:
  void setup() override;
  // AFTER_BLUETOOTH: BLE controller must initialize before esp_netif_init per IDF guidance.
  float get_setup_priority() const override { return setup_priority::AFTER_BLUETOOTH; }

  // Codegen-time priority list construction. Called once per `network: priority:` entry
  // in YAML order. The interface name pointer must have static storage duration.
  void add_priority_entry(const char *interface, uint32_t timeout_ms);

  // Currently-active interface in priority order (the one set as default netif).
  // Returns nullptr if no priority list is configured or no interface is up.
  const char *get_active_interface() const { return this->active_interface_; }
  esp_netif_t *get_active_netif() const { return this->active_netif_; }

 protected:
  // Maps a YAML interface name to its ESP-IDF netif if-key.
  // Returns nullptr if the interface name is not recognized.
  static const char *interface_to_ifkey_(const char *interface);

  // ESP-IDF event handler trampoline. Fires on IP_EVENT_* and re-arbitrates the default netif.
  static void event_handler_(void *arg, esp_event_base_t base, int32_t id, void *data);

  // Walk priority_list_ in order. Set the highest-priority netif that is up as the
  // ESP-IDF default. No-op if priority_list_ is empty (single-interface configs).
  void update_default_netif_();

  StaticVector<NetworkPriorityEntry, MAX_NETWORK_PRIORITY_ENTRIES> priority_list_;
  const char *active_interface_{nullptr};
  esp_netif_t *active_netif_{nullptr};
};

}  // namespace esphome::network
#endif
