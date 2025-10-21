#include "event.h"

#include "esphome/core/log.h"

namespace esphome {
namespace event {

static const char *const TAG = "event";

void Event::trigger(const std::string &event_type) {
  auto found = types_.find(event_type);
  if (found == types_.end()) {
    ESP_LOGE(TAG, "'%s': invalid event type for trigger(): %s", this->get_name().c_str(), event_type.c_str());
    return;
  }
  last_event_type = &(*found);
  ESP_LOGD(TAG, "'%s' Triggered event '%s'", this->get_name().c_str(), last_event_type->c_str());
  this->event_callback_.call(event_type);
}

void Event::add_on_event_callback(std::function<void(const std::string &event_type)> &&callback) {
  this->event_callback_.add(std::move(callback));
}

}  // namespace event
}  // namespace esphome
