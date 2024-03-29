#pragma once

#include <vector>
#include <string>

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "event_traits.h"

namespace esphome {
namespace event {

#define LOG_EVENT(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
  }

/** Base class for all events.
 *
 * An event is just a momentary action that does not have a state, only a trigger.
 */
class Event : public EntityBase, public EntityBase_DeviceClass {
 public:
  EventTraits traits;

  /** Fire this event. This is called by the front-end.
   *
   * For implementing events, please override event_fired_action.
   */
  void fire_event(const std::string &event_type);

  /// Return whether this event component contains the provided event type.
  bool has_event_type(const std::string &event_type) const;

  void set_event_types(const std::vector<std::string> &event_types);

  std::vector<std::string> get_event_types() const;

  /// Return the number of event types in this event component.
  size_t size() const;

  /** Set callback for state changes.
   *
   * @param callback The void() callback.
   */
  void add_on_event_fired_callback(std::function<void(const std::string &event_type)> &&callback);

 protected:
  virtual void event_fired_action(const std::string &event_type) = 0;

  CallbackManager<void(const std::string &event_type)> event_fired_callback_;
};

}  // namespace event
}  // namespace esphome
