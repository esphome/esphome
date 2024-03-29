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
  this->event_fired_action(event_type);
  this->event_fired_callback_.call(event_type);
}

void Event::add_on_event_fired_callback(std::function<void(const std::string &event_type)> &&callback) {
  this->event_fired_callback_.add(std::move(callback));
}

bool Event::has_event_type(const std::string &event_type) const {
  auto event_types = traits.get_types();
  return std::find(event_types.begin(), event_types.end(), event_type) != event_types.end();
}

void Event::set_event_types(const std::vector<std::string> &event_types) {
  this->traits.set_types(event_types);
}

std::vector<std::string> Event::get_event_types() const { return this->traits.get_types(); }

size_t Event::size() const { return this->traits.get_types().size(); }

}  // namespace event
}  // namespace esphome
