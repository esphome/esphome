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
void HomeassistantTime::setup() {
  global_homeassistant_time = this;

  this->set_interval(15 * 60 * 1000, []() {
    // re-request time every 15 minutes
    api::global_api_server->request_time();
  });
}

HomeassistantTime *global_homeassistant_time = nullptr;

bool GetTimeResponse::decode_32bit(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 1:
      // fixed32 epoch_seconds = 1;
      if (global_homeassistant_time != nullptr) {
        global_homeassistant_time->set_epoch_time(value);
      }
      return true;
    default:
      return false;
  }
}
api::APIMessageType GetTimeResponse::message_type() const { return api::APIMessageType::GET_TIME_RESPONSE; }

}  // namespace homeassistant
}  // namespace esphome
