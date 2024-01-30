#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/time.h"
#include "esphome/core/entity_base.h"

#include "datetime_call.h"
#include "datetime_traits.h"

#include <vector>
#include <regex>

namespace esphome {
namespace datetime {

#define LOG_DATETIME(prefix, type, obj) \
  if ((obj) != nullptr) { \
    ESP_LOGCONFIG(TAG, "%s%s '%s'", prefix, LOG_STR_LITERAL(type), (obj)->get_name().c_str()); \
    if (!(obj)->get_icon().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Icon: '%s'", prefix, (obj)->get_icon().c_str()); \
    } \
    if (!(obj)->traits.get_device_class().empty()) { \
      ESP_LOGCONFIG(TAG, "%s  Device Class: '%s'", prefix, (obj)->traits.get_device_class().c_str()); \
    } \
  }

#define STRFTIME_FORMAT_FROM_OBJ(obj, web) \
  (obj)->has_date && (obj)->has_time ? (web) ? "%FT%T" : "%F %T" : (obj)->has_date ? "%F" : (obj)->has_time ? "%T" : ""

// std::regex time_regex(R"(^(\d{4}-\d{2}-\d{2}(?:[ T]\d{2}:\d{2}(:\d{2})?)?|\d{2}:\d{2}(:\d{2})?)$)");

class DatetimeTimeTrigger;

class Datetime : public EntityBase {
 public:
  explicit Datetime();

 public:
  ESPTime state_as_time;
  std::string state;
  bool has_date;
  bool has_time;

  void publish_state(std::string state);

  DatetimeCall make_call() { return DatetimeCall(this); }

  void add_on_state_callback(std::function<void(std::string)> &&callback);

  DatetimeTraits traits;

  /// Return whether this Datetime has gotten a full state yet.
  bool has_state() const { return has_state_; }

 protected:
  friend class DatetimeCall;
  friend class DatetimeTimeTrigger;

  /** Set the value of the Datetime, this is a virtual method that each number integration must implement.
   *
   * This method is called by the DatetimeCall.
   *
   * @param value The value as validated by the DatetimeCall.
   */
  virtual void control(std::string) = 0;

  CallbackManager<void(std::string)> state_callback_;
  bool has_state_{false};
};

}  // namespace datetime
}  // namespace esphome
