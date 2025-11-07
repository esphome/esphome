#pragma once

#include <cstring>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace event {

#define LOG_EVENT(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon_ref().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon_ref().c_str()); \
    } \
    if (!(obj)->get_device_class_ref().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Device Class: '%s'", prefix, (obj)->get_device_class_ref().c_str()); \
    } \
  }

class Event : public EntityBase, public EntityBase_DeviceClass {
 public:
  const char *last_event_type{nullptr};

  void trigger(const std::string &event_type);

  /// Set the event types supported by this event (from initializer list).
  void set_event_types(std::initializer_list<const char *> event_types) { this->types_ = event_types; }
  /// Set the event types supported by this event (from FixedVector).
  void set_event_types(const FixedVector<const char *> &event_types) { this->types_ = event_types; }
  /// Set the event types supported by this event (from C array).
  template<size_t N> void set_event_types(const char *const (&event_types)[N]) {
    this->types_.assign(event_types, event_types + N);
  }

  // Deleted overloads to catch incorrect std::string usage at compile time with clear error messages
  void set_event_types(std::initializer_list<std::string> event_types) = delete;
  void set_event_types(const FixedVector<std::string> &event_types) = delete;

  /// Return the event types supported by this event.
  const FixedVector<const char *> &get_event_types() const { return this->types_; }

  void add_on_event_callback(std::function<void(const std::string &event_type)> &&callback);

 protected:
  CallbackManager<void(const std::string &event_type)> event_callback_;
  FixedVector<const char *> types_;
};

}  // namespace event
}  // namespace esphome
