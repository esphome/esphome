#include "shutdown.h"
#include "esphome/core/log.h"
#include "esphome/core/application.h"
#include "esphome/core/hal.h"

#ifdef USE_ESP32
#include <esp_sleep.h>
#endif
#ifdef USE_ESP8266
#include <Esp.h>
#endif
#ifdef USE_ZEPHYR
#include <zephyr/sys/poweroff.h>
#endif
#ifdef USE_HOST
#include <cstdlib>
#endif

namespace esphome::shutdown {

ESPHOME_LOG_TAG(TAG, "shutdown");

void shutdown_device() {
  ESP_LOGI(TAG, "Shutting down");
  App.safe_shutdown(TEARDOWN_TIMEOUT_REBOOT_MS);
#if defined(USE_ESP8266)
  ESP.deepSleep(0);  // NOLINT(readability-static-accessed-through-instance)
#elif defined(USE_ESP32)
  esp_deep_sleep_start();
#elif defined(USE_BK72XX)
  // No wake source is set, so only a reset wakes it
  lt_deep_sleep_enter();
#elif defined(USE_ZEPHYR) && defined(CONFIG_POWEROFF)
  sys_poweroff();
#elif defined(USE_HOST)
  exit(0);  // NOLINT(concurrency-mt-unsafe)
#else
  // No power off on this chip: the main loop stops here until a reset
  while (true) {
    arch_feed_wdt();
    delay(100);  // NOLINT
  }
#endif
}

}  // namespace esphome::shutdown
