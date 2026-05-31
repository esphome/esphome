#include "network_component.h"

#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && defined(USE_ESP32)
#include "esphome/core/log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"
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

}  // namespace esphome::network
#endif
