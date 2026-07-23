#include "network_component.h"

#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && defined(USE_ESP32)
#include "esphome/core/log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"

#if defined(USE_NETWORK_DEFAULT_ROUTE) && defined(USE_ESP_IDF)
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

#if defined(USE_NETWORK_DEFAULT_ROUTE) && defined(USE_ESP_IDF)
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
  // The first connected interface in the user's network: priority: order carries
  // the default route. Without this, ESP-IDF selects by fixed route_prio values
  // (WiFi STA 100 > Ethernet 50), which inverts an ethernet-first list whenever
  // both interfaces are up. Polling is_connected() also covers runtime
  // enable()/disable() and link loss, which all update that state.
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
  // Keep the last default while nothing is connected: there is no better
  // candidate, and clearing it would only churn lwIP state.
  if (best == nullptr || best == this->default_netif_)
    return;
  this->default_netif_ = best;
  esp_netif_set_default_netif(best);
  ESP_LOGI(TAG, "Default interface: %s", esp_netif_get_desc(best));
}
#endif  // USE_NETWORK_DEFAULT_ROUTE && USE_ESP_IDF

}  // namespace esphome::network
#endif
