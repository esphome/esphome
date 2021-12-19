#include "shutdown_button.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

#ifdef USE_ESP32
#include <esp_sleep.h>
#endif
#ifdef USE_ESP8266
#include <Esp.h>
#endif

namespace esphome {
namespace shutdown {

static const char *const TAG = "shutdown.button";

void ShutdownButton::press_action() {
  ESP_LOGI(TAG, "Shutting down...");
  delay(100);  // NOLINT

  App.run_safe_shutdown_hooks();
#ifdef USE_ESP8266
  ESP.deepSleep(0);  // NOLINT(readability-static-accessed-through-instance)
#endif
#ifdef USE_ESP32
  esp_deep_sleep_start();
#endif
}
void ShutdownButton::dump_config() { LOG_BUTTON("", "Shutdown Button", this); }

}  // namespace shutdown
}  // namespace esphome
