#include "event.h"

#include "esphome/core/log.h"

namespace esphome {
namespace event {

static const char *const TAG = "event";

void Event::trigger(const std::string &event_type) {
  // Linear search with strcmp - faster than std::set for small datasets (1-5 items typical)
  const char *found = nullptr;
  for (const char *type : this->types_) {
    if (strcmp(type, event_type.c_str()) == 0) {
      found = type;
      break;
    }
  }
  if (found == nullptr) {
    ESP_LOGE(TAG, "'%s': invalid event type for trigger(): %s", this->get_name().c_str(), event_type.c_str());
    return;
  }
  this->last_event_type = found;
  ESP_LOGD(TAG, "'%s' Triggered event '%s'", this->get_name().c_str(), this->last_event_type);
  this->event_callback_.call(event_type);
}

void Event::add_on_event_callback(std::function<void(const std::string &event_type)> &&callback) {
  this->event_callback_.add(std::move(callback));
}

}  // namespace event
}  // namespace esphome
