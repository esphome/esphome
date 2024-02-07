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

class DatetimeTimeTrigger;

class Datetime : public EntityBase {
 public:
  explicit Datetime();

  ESPTime state_as_time;
  std::string state;
  bool has_date;
  bool has_time;

  void publish_state(const std::string &state);

  DatetimeCall make_call() { return DatetimeCall(this); }

  void add_on_state_callback(std::function<void(std::string)> &&callback);
  void add_on_state_callback(std::function<void(ESPTime)> &&callback);

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
  CallbackManager<void(ESPTime)> state_callback_time_;
  bool has_state_{false};
};

}  // namespace datetime
}  // namespace esphome
