#include "esp_utils.h"

#include "esphome/core/defines.h"
#if defined(USE_NETWORK) && defined(USE_ESP32)
#include "esphome/core/log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_event.h"
namespace esphome {
namespace network {

static const char *const TAG = "network_esp";

static bool initialized = false;

bool esp_init() {
  if (initialized) {
    return true;
  }
  esp_err_t err;
  err = esp_netif_init();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_netif_init failed: (%d) %s", err, esp_err_to_name(err));
    return false;
  }
  err = esp_event_loop_create_default();
  if (err != ESP_OK) {
    ESP_LOGE(TAG, "esp_event_loop_create_default failed: (%d) %s", err, esp_err_to_name(err));
    return false;
  }
  initialized = true;
  return true;
}

}  // namespace network
}  // namespace esphome
#endif
