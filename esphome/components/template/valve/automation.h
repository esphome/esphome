#pragma once

#include "template_valve.h"

#include "esphome/core/automation.h"

namespace esphome {
namespace template_ {

template<typename... Ts> class TemplateValvePublishAction : public Action<Ts...>, public Parented<TemplateValve> {
  TEMPLATABLE_VALUE(float, position)
  TEMPLATABLE_VALUE(valve::ValveOperation, current_operation)

  void play(Ts... x) override {
    if (this->position_.has_value())
      this->parent_->position = this->position_.value(x...);
    if (this->current_operation_.has_value())
      this->parent_->current_operation = this->current_operation_.value(x...);
    this->parent_->publish_state();
  }
};

}  // namespace template_
}  // namespace esphome
