#include "homeassistant_time.h"
#include "esphome/core/log.h"

namespace esphome::homeassistant {

ESPHOME_LOG_TAG(TAG, "homeassistant.time");

void HomeassistantTime::dump_config() {
  ESP_LOGCONFIG(TAG, "Home Assistant Time");
  RealTimeClock::dump_config();
}

void HomeassistantTime::setup() { global_homeassistant_time = this; }

void HomeassistantTime::update() { api::global_api_server->request_time(); }

HomeassistantTime *global_homeassistant_time = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace esphome::homeassistant
