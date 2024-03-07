#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/time.h"

namespace esphome {
namespace datetime {

class DateTimeBase : public EntityBase {
 public:
  /// Return whether this Datetime has gotten a full state yet.
  bool has_state() const { return this->has_state_; }

  virtual ESPTime state_as_esptime() const = 0;

  void add_on_state_callback(std::function<void()> &&callback) { this->state_callback_.add(std::move(callback)); }

 protected:
  CallbackManager<void()> state_callback_;

  bool has_state_{false};
};

class DateTimeStateTrigger : public Trigger<ESPTime> {
 public:
  explicit DateTimeStateTrigger(DateTimeBase *parent) {
    parent->add_on_state_callback([this, parent]() { this->trigger(parent->state_as_esptime()); });
  }
};

}  // namespace datetime
}  // namespace esphome
