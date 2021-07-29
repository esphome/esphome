#include "esphome/core/application.h"

#ifdef ARDUINO_ARCH_ESP32

#include <esp_task_wdt.h>

namespace esphome {

static const char *const TAG = "app_esp32";

void ICACHE_RAM_ATTR HOT Application::feed_wdt_arch_() { yield(); }

void Application::pre_setup_arch_() {
  // Increase timeout during setup() time to avoid task watchdog resets.
  esp_task_wdt_init(30, true);
}

void Application::post_setup_arch_() { esp_task_wdt_init(5, true); }

}  // namespace esphome
#endif
