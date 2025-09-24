#include "mqtt_update.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_UPDATE

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.update";

using namespace esphome::update;

MQTTUpdateComponent::MQTTUpdateComponent(UpdateEntity *update) : update_(update) {}

void MQTTUpdateComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &payload) {
    if (payload == "INSTALL") {
      this->update_->perform();
    } else {
      ESP_LOGW(TAG, "'%s': Received unknown update payload: %s", this->friendly_name().c_str(), payload.c_str());
      this->status_momentary_warning("state", 5000);
    }
  });

  this->update_->add_on_state_callback([this]() { this->defer("send", [this]() { this->publish_state(); }); });
}

bool MQTTUpdateComponent::publish_state() {
  return this->publish_json(this->get_state_topic_(), [this](JsonObject root) {
    root["installed_version"] = this->update_->update_info.current_version;
    root["latest_version"] = this->update_->update_info.latest_version;
    root["title"] = this->update_->update_info.title;
    if (!this->update_->update_info.summary.empty())
      root["release_summary"] = this->update_->update_info.summary;
    if (!this->update_->update_info.release_url.empty())
      root["release_url"] = this->update_->update_info.release_url;
  });
}

void MQTTUpdateComponent::send_discovery(JsonObject root, mqtt::SendDiscoveryConfig &config) {
  root["schema"] = "json";
  root[MQTT_PAYLOAD_INSTALL] = "INSTALL";
}

bool MQTTUpdateComponent::send_initial_state() { return this->publish_state(); }

void MQTTUpdateComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Update '%s': ", this->update_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true);
}

std::string MQTTUpdateComponent::component_type() const { return "update"; }
const EntityBase *MQTTUpdateComponent::get_entity() const { return this->update_; }

}  // namespace mqtt
}  // namespace esphome

#endif  // USE_UPDATE
#endif  // USE_MQTT
