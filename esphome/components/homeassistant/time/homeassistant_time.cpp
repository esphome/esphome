#include "homeassistant_time.h"
#include "esphome/core/log.h"

namespace esphome {
namespace homeassistant {

static const char *TAG = "homeassistant.time";

void HomeassistantTime::dump_config() {
  ESP_LOGCONFIG(TAG, "Home Assistant Time:");
  ESP_LOGCONFIG(TAG, "  Timezone: '%s'", this->timezone_.c_str());
}

float HomeassistantTime::get_setup_priority() const { return setup_priority::DATA; }

void HomeassistantTime::setup() { global_homeassistant_time = this; }

void HomeassistantTime::update() { api::global_api_server->request_time(); }

HomeassistantTime *global_homeassistant_time = nullptr;
}  // namespace homeassistant
}  // namespace esphome
