#pragma once

#include "esphome/components/number/number.h"
#include "esphome/core/component.h"
#include "esphome/core/preferences.h"

namespace esphome::lvgl {

class LVGLNumber final : public number::Number, public Component {
 public:
  LVGLNumber(void (*control_lambda)(float), float (*value_lambda)(), bool restore)
      : control_lambda_(control_lambda), value_lambda_(value_lambda), restore_(restore) {}

  void setup() override {
    float value = this->value_lambda_();
    if (this->restore_) {
      this->pref_ = this->make_entity_preference<float>();
      if (this->pref_.load(&value)) {
        this->control_lambda_(value);
      }
    }
    this->publish_state(value);
  }

  void on_value() { this->publish_(this->value_lambda_()); }

 protected:
  void publish_(float value) {
    this->publish_state(value);
    if (this->restore_)
      this->pref_.save(&value);
  }
  void control(float value) override {
    this->control_lambda_(value);
    this->publish_(value);
  }
  void (*control_lambda_)(float);
  float (*value_lambda_)();
  bool restore_;
  ESPPreferenceObject pref_{};
};

}  // namespace esphome::lvgl
