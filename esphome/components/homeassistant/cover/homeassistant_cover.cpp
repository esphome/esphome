#include "homeassistant_cover.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace homeassistant {

static const char *const TAG = "homeassistant.cover";

static const std::string CURRENT_POSITION = "current_position";

using namespace esphome::cover;

void HomeassistantCover::setup() {
  this->is_unknown_ = true;
  this->is_unavailable_ = true;

  // state
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, optional<std::string>(""), [this](const std::string &state) {
        auto val = parse_cover(state.c_str());

        this->is_unknown_ = false;
        this->is_unavailable_ = false;

        if (val == PARSE_COVER_OPENING) {
          this->current_operation = COVER_OPERATION_OPENING;
        } else if (val == PARSE_COVER_CLOSING) {
          this->current_operation = COVER_OPERATION_CLOSING;
        } else {
          this->current_operation = COVER_OPERATION_IDLE;
          if (val == PARSE_COVER_UNKNOWN) {
            this->is_unknown_ = true;
          } else if (val == PARSE_COVER_UNAVAILABLE) {
            this->is_unknown_ = true;
            this->is_unavailable_ = true;
          }
        }

        ESP_LOGD(TAG, "'%s': Got state %s", this->entity_id_.c_str(), state.c_str());
        this->publish_state();
        return;
      });

  // current_position
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, CURRENT_POSITION, [this](const std::string &state) {
        auto val = parse_number<float>(state);
        if (!val.has_value()) {
          ESP_LOGW(TAG, "'%s': Can't convert '%s' to number!", this->entity_id_.c_str(), state.c_str());
          this->position = NAN;
          return;
        }

        ESP_LOGD(TAG, "'%s': Got position %.2f%%", this->entity_id_.c_str(), *val);
        this->position = (*val) / 100;
        this->publish_state();
        return;
      });
}
void HomeassistantCover::dump_config() {
  LOG_COVER("", "Homeassistant Cover", this);
  ESP_LOGCONFIG(TAG, "  Entity ID: '%s'", this->entity_id_.c_str());
}
float HomeassistantCover::get_setup_priority() const { return setup_priority::AFTER_CONNECTION; }

cover::CoverTraits HomeassistantCover::get_traits() {
  auto traits = CoverTraits();
  traits.set_supports_stop(true);
  traits.set_supports_position(true);
  traits.set_supports_tilt(false);
  traits.set_is_assumed_state(false);
  return traits;
}

void HomeassistantCover::control(const CoverCall &call) {
  if (!api::global_api_server) {
    ESP_LOGE(TAG, "Missing API Server");
    return;
  }
  if (!api::global_api_server->is_connected()) {
    ESP_LOGE(TAG, "API Server not connected");
    return;
  }
  api::HomeassistantServiceResponse resp;
  api::HomeassistantServiceMap entity_id_kv;
  entity_id_kv.key = "entity_id";
  entity_id_kv.value = this->entity_id_;
  resp.data.push_back(entity_id_kv);

  if (call.get_stop()) {
    resp.service = "cover.stop_cover";
  } else if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    resp.service = "cover.set_cover_position";
    api::HomeassistantServiceMap position_kv;
    position_kv.key = "position";
    position_kv.value = to_string(uint8_t(pos * 100));
    resp.data.push_back(position_kv);
  } else {
    ESP_LOGW(TAG, "control called with unknown action");
    return;
  }

  api::global_api_server->send_homeassistant_service_call(resp);
}

bool HomeassistantCover::is_unavailable() { return this->is_unavailable_; }
bool HomeassistantCover::is_unknown() { return this->is_unknown_; }

}  // namespace homeassistant
}  // namespace esphome
