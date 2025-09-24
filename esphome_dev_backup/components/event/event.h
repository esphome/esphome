#pragma once

#include <set>
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
    if (!(obj)->get_device_class().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Device Class: '%s'", prefix, (obj)->get_device_class().c_str()); \
    } \
  }

class Event : public EntityBase, public EntityBase_DeviceClass {
 public:
  const std::string *last_event_type;

  void trigger(const std::string &event_type);
  void set_event_types(const std::set<std::string> &event_types) { this->types_ = event_types; }
  std::set<std::string> get_event_types() const { return this->types_; }
  void add_on_event_callback(std::function<void(const std::string &event_type)> &&callback);

 protected:
  CallbackManager<void(const std::string &event_type)> event_callback_;
  std::set<std::string> types_;
};

}  // namespace event
}  // namespace esphome
