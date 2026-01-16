#pragma once

#include <cstring>
#include <limits>
#include <string>
#include <vector>

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "esphome/core/string_ref.h"

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
  void trigger(const std::string &event_type);

  /// Set the event types supported by this event (from initializer list).
  void set_event_types(std::initializer_list<const char *> event_types) {
    this->types_ = event_types;
    this->last_event_type_ = nullptr;  // Reset when types change
  }
  /// Set the event types supported by this event (from FixedVector).
  void set_event_types(const FixedVector<const char *> &event_types);
  /// Set the event types supported by this event (from vector).
  void set_event_types(const std::vector<const char *> &event_types);

  // Deleted overloads to catch incorrect std::string usage at compile time with clear error messages
  void set_event_types(std::initializer_list<std::string> event_types) = delete;
  void set_event_types(const FixedVector<std::string> &event_types) = delete;
  void set_event_types(const std::vector<std::string> &event_types) = delete;

  /// Return the event types supported by this event.
  const FixedVector<const char *> &get_event_types() const { return this->types_; }

  /// Return the last triggered event type, or empty StringRef if no event triggered yet.
  StringRef get_last_event_type() const { return StringRef::from_maybe_nullptr(this->last_event_type_); }

  /// Return event type by index, or nullptr if index is out of bounds.
  const char *get_event_type(uint8_t index) const {
    return index < this->types_.size() ? this->types_[index] : nullptr;
  }

  /// Return index of last triggered event type, or max uint8_t if no event triggered yet.
  uint8_t get_last_event_type_index() const {
    if (this->last_event_type_ == nullptr)
      return std::numeric_limits<uint8_t>::max();
    // Most events have <3 types, uint8_t is sufficient for all reasonable scenarios
    const uint8_t size = static_cast<uint8_t>(this->types_.size());
    for (uint8_t i = 0; i < size; i++) {
      if (this->types_[i] == this->last_event_type_)
        return i;
    }
    return std::numeric_limits<uint8_t>::max();
  }

  /// Check if an event has been triggered.
  bool has_event() const { return this->last_event_type_ != nullptr; }

  void add_on_event_callback(std::function<void(const std::string &event_type)> &&callback);

 protected:
  LazyCallbackManager<void(const std::string &event_type)> event_callback_;
  FixedVector<const char *> types_;

 private:
  /// Last triggered event type - must point to entry in types_ to ensure valid lifetime.
  /// Set by trigger() after validation, reset to nullptr when types_ changes.
  const char *last_event_type_{nullptr};
};

}  // namespace event
}  // namespace esphome
