#include "network_component.h"

#include "esphome/core/defines.h"
#ifdef USE_NETWORK
#include "esphome/core/log.h"
#ifdef USE_ESP32
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"
#endif
namespace esphome {
namespace network {

static const char *const TAG = "network";

void NetworkComponent::setup() {
  // Initialize ESP-IDF network interfaces and ensure the default event loop exists.
#ifdef USE_ESP32
  esp_err_t err;
  err = esp_netif_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_netif_init failed: (%d) %s", err, esp_err_to_name(err));
    this->mark_failed();
    return;
  }
  err = esp_event_loop_create_default();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_event_loop_create_default failed: (%d) %s", err, esp_err_to_name(err));
    this->mark_failed();
    return;
  }
#endif
}  // namespace network
}  // namespace esphome
#endif
