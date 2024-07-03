#include "homeassistant_cover.h"
#include "esphome/core/log.h"
#include "esphome/components/api/api_server.h"

namespace esphome {
namespace homeassistant {

static const char *const TAG = "homeassistant.cover";

static const std::string CURRENT_POSITION = "current_position";
static const std::string CURRENT_TILT_POSITION = "current_tilt_position";

using namespace esphome::cover;

void HomeassistantCover::setup() {
  // state
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, optional<std::string>(""), [this](const std::string &state) {
        if (this->handle_state(state.c_str())) {
          ESP_LOGD(TAG, "'%s': Got entity state %s", this->entity_id_.c_str(), state.c_str());
          this->publish_state();
          return;
        }

        auto val = parse_cover(state.c_str());
        if (val == PARSE_COVER_OPENING) {
          this->current_operation = COVER_OPERATION_OPENING;
        } else if (val == PARSE_COVER_CLOSING) {
          this->current_operation = COVER_OPERATION_CLOSING;
        } else {
          this->current_operation = COVER_OPERATION_IDLE;
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

  // current_tilt_position
  api::global_api_server->subscribe_home_assistant_state(
      this->entity_id_, CURRENT_TILT_POSITION, [this](const std::string &state) {
        auto val = parse_number<float>(state);
        if (!val.has_value()) {
          ESP_LOGW(TAG, "'%s': Can't convert '%s' to number!", this->entity_id_.c_str(), state.c_str());
          this->tilt = NAN;
          return;
        }

        ESP_LOGD(TAG, "'%s': Got tilt %.2f%%", this->entity_id_.c_str(), *val);
        this->tilt = (*val) / 100;
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
  traits.set_supports_toggle(true);
  traits.set_supports_tilt(true);
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
  if (this->is_unavailable()) {
    ESP_LOGE(TAG, "Entity has state unavailable");
    return;
  }
  api::HomeassistantServiceResponse resp;
  api::HomeassistantServiceMap entity_id_kv;
  entity_id_kv.key = "entity_id";
  entity_id_kv.value = this->entity_id_;
  resp.data.push_back(entity_id_kv);

  if (call.get_stop()) {
    resp.service = "cover.stop_cover";
  } else if (call.get_toggle()) {
    resp.service = "cover.toggle";
  } else if (call.get_position().has_value()) {
    auto pos = *call.get_position();
    resp.service = "cover.set_cover_position";
    api::HomeassistantServiceMap position_kv;
    position_kv.key = "position";
    position_kv.value = to_string(uint8_t(pos * 100));
    resp.data.push_back(position_kv);
  } else if (call.get_tilt().has_value()) {
    auto tilt = *call.get_tilt();
    resp.service = "cover.set_cover_tilt_position";
    api::HomeassistantServiceMap position_kv;
    position_kv.key = "tilt_position";
    position_kv.value = to_string(uint8_t(tilt * 100));
    resp.data.push_back(position_kv);
  } else {
    ESP_LOGW(TAG, "control called with unknown action");
    return;
  }

  api::global_api_server->send_homeassistant_service_call(resp);
}

}  // namespace homeassistant
}  // namespace esphome
