#pragma once

#ifdef USE_ESP32
#include "esphome/components/select/select.h"
#include "esphome/core/string_ref.h"
#include "esphome/core/log.h"
#include "esphome/components/fendt_caravan/caravan_sensor_base.h"
#include "esphome/components/fendt_caravan/variable.h"

namespace esphome::fendt_caravan {

#define FENDT_SELECT(name) \
 protected: \
  FendtSelect *name##_select_{nullptr}; \
\
 public: \
  void set_##name##_select(FendtSelect *select) { this->name##_select_ = select; }

class FendtSelect : public CaravanSensorBase<std::string>, public select::Select {
 public:
  void set_state_change_callback(std::function<void(FendtSelect *, std::string state)> &&callback) {
    this->on_state_change_.add(std::move(callback));
  }

 protected:
  void control(const std::string &value) override {
    if (this->variable_)
      this->variable_->set_value(value);
    this->on_state_change_.call(this, value);
  }
  void on_decoded(std::string value) override { this->publish_state(value); }

 private:
  CallbackManager<void(FendtSelect *, std::string state)> on_state_change_{};
};
}  // namespace esphome::fendt_caravan
#endif
