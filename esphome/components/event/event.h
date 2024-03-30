#pragma once

#include <vector>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace event {

#define LOG_EVENT(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
  }

class Event : public EntityBase, public EntityBase_DeviceClass {
 public:
  void fire_event(const std::string &event_type);
  bool has_event_type(const std::string &event_type) const;
  void set_event_types(const std::vector<std::string> &event_types);
  std::vector<std::string> get_event_types() const { return this->types_; }
  void add_on_event_fired_callback(std::function<void(const std::string &event_type)> &&callback);
 protected:
  CallbackManager<void(const std::string &event_type)> event_fired_callback_;
  std::vector<std::string> types_;
};

}  // namespace event
}  // namespace esphome
