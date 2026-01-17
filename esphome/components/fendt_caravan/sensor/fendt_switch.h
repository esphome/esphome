#pragma once

#ifdef USE_ESP32
#include "esphome/components/switch/switch.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"
#include "fendt_component.h"
#include "variable.h"

namespace esphome {
namespace fendt_caravan {

#define FENDT_SWITCH(name) \
 protected: \
  FendtSwitch *name##_switch_{nullptr}; \
\
 public: \
  void set_##name##_switch(FendtSwitch *s) { this->name##_switch_ = s; }

class FendtSwitch : public FendtComponent<bool>, public switch_::Switch {
 public:
  void set_state_change_callback(std::function<void(FendtSwitch *, bool state)> &&callback) {
    this->on_state_change_.add(std::move(callback));
  }

 protected:
  void write_state(bool state) override {
    if (this->variable_)
      this->variable_->set_value(state);
    this->on_state_change_.call(this, state);
  }
  void on_decoded(const bool value) override { this->publish_state(value); }
  CallbackManager<void(FendtSwitch *, bool state)> on_state_change_{};

 private:
  const char *const tag_ = "FSW";
};

}  // namespace fendt_caravan
}  // namespace esphome
#endif
