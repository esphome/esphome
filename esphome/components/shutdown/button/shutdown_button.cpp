#include "shutdown_button.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

#ifdef USE_ESP32
#include <esp_sleep.h>
#endif
#ifdef USE_ESP8266
#include <Esp.h>
#endif

namespace esphome::shutdown {

ESPHOME_LOG_TAG(TAG, "shutdown.button");

void shutdown_device() {
  ESP_LOGI(TAG, "Shutting down");
  // Let MQTT settle a bit
  delay(100);  // NOLINT
  App.run_safe_shutdown_hooks();
  App.teardown_components(TEARDOWN_TIMEOUT_REBOOT_MS);
  App.run_powerdown_hooks();
#ifdef USE_ESP8266
  ESP.deepSleep(0);  // NOLINT(readability-static-accessed-through-instance)
#endif
#ifdef USE_ESP32
  esp_deep_sleep_start();
#endif
}

}  // namespace esphome::shutdown
