#pragma once

#ifdef USE_ESP32
#include "esphome/components/number/number.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"
#include "esphome/components/fendt_caravan/fendt_component.h"
#include "esphome/components/fendt_caravan/variable.h"

namespace esphome::fendt_caravan {

#define FENDT_NUMBER(name) \
 protected: \
  FendtNumber *name##_number_{nullptr}; \
\
 public: \
  void set_##name##_number(FendtNumber *number) { this->name##_number_ = number; }

class FendtNumber : public FendtComponent<float>, public number::Number {
 public:
  void set_state_change_callback(std::function<void(FendtNumber *, float state)> &&callback) {
    this->on_state_change_.add(std::move(callback));
  }

 protected:
  void control(float value) override {
    if (this->variable_)
      this->variable_->set_value(value);
    this->on_state_change_.call(this, value);
  }
  void on_decoded(const float value) override { this->publish_state(value); }

 private:
  CallbackManager<void(FendtNumber *, float state)> on_state_change_{};
};
}  // namespace esphome::fendt_caravan

#endif
