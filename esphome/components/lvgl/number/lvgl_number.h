#pragma once

#include <utility>

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome {
namespace lvgl {

class LVGLNumber : public number::Number, public Component {
 public:
  LVGLNumber(std::function<void(float)> control_lambda, std::function<float()> value_lambda, lv_event_code_t event,
             bool restore)
      : control_lambda_(std::move(control_lambda)),
        value_lambda_(std::move(value_lambda)),
        event_(event),
        restore_(restore) {}

  void setup() override {
    float value = this->value_lambda_();
    if (this->restore_) {
      this->pref_ = global_preferences->make_preference<float>(this->get_object_id_hash());
      if (this->pref_.load(&value)) {
        this->control_lambda_(value);
      }
    }
    this->publish_state(value);
  }

  void on_value() { this->publish_state(this->value_lambda_()); }

 protected:
  void control(float value) override {
    this->control_lambda_(value);
    this->publish_state(value);
    if (this->restore_)
      this->pref_.save(&value);
  }
  std::function<void(float)> control_lambda_;
  std::function<float()> value_lambda_;
  lv_event_code_t event_;
  bool restore_;
  ESPPreferenceObject pref_{};
};

}  // namespace lvgl
}  // namespace esphome
