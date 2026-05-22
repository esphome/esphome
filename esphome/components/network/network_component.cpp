#include "network_component.h"

#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && defined(USE_ESP32)
#include "esphome/core/log.h"

#include <cstring>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"

namespace esphome::network {

static const char *const TAG = "network";

void NetworkComponent::setup() {
  // Initialize ESP-IDF network interfaces and ensure the default event loop exists
  esp_err_t err;
  err = esp_netif_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_netif_init failed: (%d) %s", err, esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  err = esp_event_loop_create_default();
  // ESP_ERR_INVALID_STATE is returned if the default loop already exists,
  // which is fine since we just want to make sure it exists
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    ESP_LOGE(TAG, "esp_event_loop_create_default failed: (%d) %s", err, esp_err_to_name(err));
    this->mark_failed();
    return;
  }

  // Register an IP_EVENT handler so we can re-arbitrate the default netif on every
  // interface up/down. ESP-IDF's built-in auto-selection picks by route_prio (WiFi STA = 100
  // > Ethernet = 50), which inverts the user's stated priority for same-subnet configurations.
  // We register AFTER esp-idf's internal handler, so our esp_netif_set_default_netif() call
  // wins and stays sticky thanks to esp-idf's "manual override" flag.
  err = esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &NetworkComponent::event_handler_, this);
  if (err != ESP_OK) {
    ESP_LOGW(TAG, "IP_EVENT handler register failed: %s — default route arbitration disabled",
             esp_err_to_name(err));
  }

  // Defensive: arbitrate now in case an interface came up before our handler registered
  // (unlikely given our AFTER_BLUETOOTH priority but cheap).
  this->update_default_netif_();
}

void NetworkComponent::add_priority_entry(const char *interface, uint32_t timeout_ms) {
  if (this->priority_list_.size() >= MAX_NETWORK_PRIORITY_ENTRIES) {
    ESP_LOGW(TAG, "Priority list full; ignoring '%s'", interface);
    return;
  }
  this->priority_list_.push_back({interface, timeout_ms});
}

const char *NetworkComponent::interface_to_ifkey_(const char *interface) {
  // Standard ESP-IDF netif keys. esphome's wifi/ethernet/openthread components create
  // netifs using these defaults.
  if (std::strcmp(interface, "ethernet") == 0)
    return "ETH_DEF";
  if (std::strcmp(interface, "wifi") == 0)
    return "WIFI_STA_DEF";  // STA carries uplink; AP never wins default route
  if (std::strcmp(interface, "openthread") == 0)
    return "OT_DEF";
  if (std::strcmp(interface, "modem") == 0)
    return "PPP_DEF";
  return nullptr;
}

void NetworkComponent::event_handler_(void *arg, esp_event_base_t /*base*/, int32_t /*id*/, void * /*data*/) {
  auto *self = static_cast<NetworkComponent *>(arg);
  self->update_default_netif_();
}

void NetworkComponent::update_default_netif_() {
  // No priority list configured → leave ESP-IDF's route_prio-based auto-selection alone.
  // Single-interface configs behave exactly as before.
  if (this->priority_list_.empty()) {
    return;
  }

  for (const auto &entry : this->priority_list_) {
    const char *ifkey = interface_to_ifkey_(entry.interface);
    if (ifkey == nullptr)
      continue;

    esp_netif_t *netif = esp_netif_get_handle_from_ifkey(ifkey);
    if (netif == nullptr)
      continue;  // component for this interface hasn't run setup() yet

    // is_netif_up returns true only when the netif has link + IP, which is what
    // we want for "this interface can carry traffic right now."
    if (!esp_netif_is_netif_up(netif))
      continue;

    if (netif != this->active_netif_) {
      ESP_LOGI(TAG, "Default interface: %s", entry.interface);
      esp_netif_set_default_netif(netif);
      this->active_interface_ = entry.interface;
      this->active_netif_ = netif;
    }
    return;
  }

  // No priority-listed interface is currently up.
  if (this->active_netif_ != nullptr) {
    ESP_LOGD(TAG, "No active interface in priority list");
    this->active_interface_ = nullptr;
    this->active_netif_ = nullptr;
    // We intentionally don't clear esp-idf's default — the next interface that comes
    // up will trigger our handler again and we'll re-pick.
  }
}

}  // namespace esphome::network
#endif
