#include "shutdown_switch.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"

namespace esphome {
namespace shutdown {

static const char *TAG = "shutdown.switch";

void ShutdownSwitch::dump_config() { LOG_SWITCH("", "Shutdown Switch", this); }
void ShutdownSwitch::write_state(bool state) {
  // Acknowledge
  this->publish_state(false);

  if (state) {
    ESP_LOGI(TAG, "Shutting down...");
    delay(100);  // NOLINT

    App.run_safe_shutdown_hooks();
#ifdef ARDUINO_ARCH_ESP8266
    ESP.deepSleep(0);
#endif
#ifdef ARDUINO_ARCH_ESP32
    esp_deep_sleep_start();
#endif
  }
}

}  // namespace shutdown
}  // namespace esphome
