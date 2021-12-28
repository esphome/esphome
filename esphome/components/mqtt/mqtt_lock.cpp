#include "mqtt_lock.h"
#include "esphome/core/log.h"

#include "mqtt_const.h"

#ifdef USE_MQTT
#ifdef USE_LOCK

namespace esphome {
namespace mqtt {

static const char *const TAG = "mqtt.lock";

using namespace esphome::lock_;

MQTTLockComponent::MQTTLockComponent(lock_::Lock *a_lock) : MQTTComponent(), lock_(a_lock) {}

void MQTTLockComponent::setup() {
  this->subscribe(this->get_command_topic_(), [this](const std::string &topic, const std::string &payload) {
    if (strcasecmp(payload.c_str(), "LOCK") == 0)
      this->lock_->lock();
    else if (strcasecmp(payload.c_str(), "UNLOCK") == 0)
      this->lock_->unlock();
    else if (strcasecmp(payload.c_str(), "OPEN") == 0)
      this->lock_->open();
    else {
      ESP_LOGW(TAG, "'%s': Received unknown status payload: %s", this->friendly_name().c_str(), payload.c_str());
      this->status_momentary_warning("state", 5000);
    }
  });
  this->lock_->add_on_state_callback(
      [this](bool enabled) { this->defer("send", [this, enabled]() { this->publish_state(enabled); }); });
}
void MQTTLockComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "MQTT Lock '%s': ", this->lock_->get_name().c_str());
  LOG_MQTT_COMPONENT(true, true);
}

std::string MQTTLockComponent::component_type() const { return "lock"; }
const EntityBase *MQTTLockComponent::get_entity() const { return this->lock_; }
void MQTTLockComponent::send_discovery(JsonObject &root, mqtt::SendDiscoveryConfig &config) {
  if (this->lock_->assumed_state())
    root[MQTT_OPTIMISTIC] = true;
}
bool MQTTLockComponent::send_initial_state() { return this->publish_state(this->lock_->state); }

bool MQTTLockComponent::publish_state(bool state) {
  const char *state_s = state ? "LOCKED" : "UNLOCKED";
  return this->publish(this->get_state_topic_(), state_s);
}

}  // namespace mqtt
}  // namespace esphome

#endif
#endif  // USE_MQTT
