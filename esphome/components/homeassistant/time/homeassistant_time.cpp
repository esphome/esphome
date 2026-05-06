#include "homeassistant_time.h"
#include "esphome/core/log.h"

namespace esphome {
namespace homeassistant {

static const char *const TAG = "homeassistant.time";

void HomeassistantTime::dump_config() {
  ESP_LOGCONFIG(TAG, "Home Assistant Time");
  RealTimeClock::dump_config();
}

void HomeassistantTime::setup() { global_homeassistant_time = this; }

void HomeassistantTime::update() { api::global_api_server->request_time(); }

HomeassistantTime *global_homeassistant_time = nullptr;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
}  // namespace homeassistant
}  // namespace esphome
