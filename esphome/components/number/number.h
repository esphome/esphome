#pragma once

#include "esphome/core/component.h"
#include "esphome/core/entity_base.h"
#include "esphome/core/helpers.h"
#include "number_call.h"
#include "number_traits.h"

namespace esphome::number {

class Number;
void log_number(const char *tag, const char *prefix, const char *type, Number *obj);

#define LOG_NUMBER(prefix, type, obj) log_number(TAG, prefix, LOG_STR_LITERAL(type), obj)

#define SUB_NUMBER(name) \
 protected: \
  number::Number *name##_number_{nullptr}; \
\
 public: \
  void set_##name##_number(number::Number *number) { this->name##_number_ = number; }

class Number;

/** Base-class for all numbers.
 *
 * A number can use publish_state to send out a new value.
 */
class Number : public EntityBase {
 public:
  float state;

  void publish_state(float state);

  NumberCall make_call() { return NumberCall(this); }

  void add_on_state_callback(std::function<void(float)> &&callback);

  NumberTraits traits;

 protected:
  friend class NumberCall;

  /** Set the value of the number, this is a virtual method that each number integration must implement.
   *
   * This method is called by the NumberCall.
   *
   * @param value The value as validated by the NumberCall.
   */
  virtual void control(float value) = 0;

  CallbackManager<void(float)> state_callback_;
};

}  // namespace esphome::number
