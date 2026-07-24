#include "network_component.h"

#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && defined(USE_ESP32)
#include "esphome/core/log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"

#ifdef USE_NETWORK_DEFAULT_ROUTE
#ifdef USE_ETHERNET
#include "esphome/components/ethernet/ethernet_component.h"
#endif
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#endif

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
}

#ifdef USE_NETWORK_DEFAULT_ROUTE
static esp_netif_t *connected_wifi_netif() {
#ifdef USE_WIFI
  auto *wifi = wifi::global_wifi_component;
  if (wifi != nullptr && wifi->is_connected())
    return wifi->get_esp_netif_sta();
#endif
  return nullptr;
}

static esp_netif_t *connected_ethernet_netif() {
#ifdef USE_ETHERNET
  auto *eth = ethernet::global_eth_component;
  if (eth != nullptr && eth->is_connected())
    return eth->get_esp_netif();
#endif
  return nullptr;
}

void NetworkComponent::loop() {
  // Pin the default route to the first connected interface in the user's priority
  // order; ESP-IDF's own route_prio selection would always favor WiFi.
  esp_netif_t *best;
#ifdef USE_NETWORK_PRIMARY_INTERFACE_WIFI
  best = connected_wifi_netif();
  if (best == nullptr)
    best = connected_ethernet_netif();
#else
  best = connected_ethernet_netif();
  if (best == nullptr)
    best = connected_wifi_netif();
#endif
  if (best == nullptr) {
    // Forget the last winner: stopping its netif cleared lwIP's default route and
    // IDF's manual override suppresses re-election, so reconnect must re-assert it.
    this->default_netif_ = nullptr;
    return;
  }
  if (best == this->default_netif_)
    return;
  if (esp_netif_set_default_netif(best) != ESP_OK)
    return;  // cache unchanged so the next iteration retries
  this->default_netif_ = best;
  ESP_LOGI(TAG, "Default interface: %s", esp_netif_get_desc(best));
}
#endif  // USE_NETWORK_DEFAULT_ROUTE

}  // namespace esphome::network
#endif
