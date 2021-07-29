#include "esphome/core/application.h"

#ifdef ARDUINO_ARCH_ESP32

namespace esphome {

static const char *const TAG = "app_esp32";

void ICACHE_RAM_ATTR HOT Application::feed_wdt_arch_() { yield(); }

}  // namespace esphome
#endif
