#include "esphome/core/application.h"

#ifdef ARDUINO_ARCH_ESP8266

namespace esphome {

static const char *const TAG = "app_esp8266";

void ICACHE_RAM_ATTR HOT Application::feed_wdt_arch_() { ESP.wdtFeed(); }

void Application::pre_setup_arch_() {}
void Application::post_setup_arch_() {}

}  // namespace esphome
#endif
