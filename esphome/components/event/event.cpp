#include "event.h"

#include "esphome/core/log.h"

namespace esphome {
namespace event {

static const char *const TAG = "event";

void Event::fire_event(const std::string &event_type) {
  if (!this->has_event_type(event_type)) {
    ESP_LOGE(TAG, "'%s': invalid event type for fire_event(): %s", this->get_name().c_str(), event_type.c_str());
    return;
  }
  ESP_LOGD(TAG, "'%s' Fired event '%s'", this->get_name().c_str(), event_type.c_str());
  this->event_fired_callback_.call(event_type);
}

bool Event::has_event_type(const std::string &event_type) const {
  return std::find(types_.begin(), types_.end(), event_type) != types_.end();
}

void Event::set_event_types(const std::vector<std::string> &types) { this->types_ = std::move(types); }

void Event::add_on_event_fired_callback(std::function<void(const std::string &event_type)> &&callback) {
  this->event_fired_callback_.add(std::move(callback));
}

}  // namespace event
}  // namespace esphome
