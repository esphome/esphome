#include "network_component.h"
#ifdef USE_NETWORK
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include "esp_event.h"
#include "esp_netif.h"
#endif

namespace esphome {
namespace network {

static const char *const TAG = "network";

void NetworkComponent::setup() {
#ifdef USE_ESP32
  // Initialize network stack early - required before web_server can bind.
  // This must run before WiFi/Ethernet setup so web_server can register
  // its handlers before captive_portal.
  esp_err_t err = esp_netif_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(err));
  }
  err = esp_event_loop_create_default();
  if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
    // ESP_ERR_INVALID_STATE means it was already created
    ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(err));
  }
#endif
}

float NetworkComponent::get_setup_priority() const {
  // Run before web_server (WIFI + 0.5) and WiFi (WIFI)
  return setup_priority::WIFI + 2.0f;
}

}  // namespace network
}  // namespace esphome
#endif
