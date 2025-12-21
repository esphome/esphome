#pragma once

#include "template_water_heater.h"
#include "esphome/core/automation.h"

namespace esphome::template_ {

template<typename... Ts>
class TemplateWaterHeaterPublishAction : public Action<Ts...>, public Parented<TemplateWaterHeater> {
 public:
  TEMPLATABLE_VALUE(float, current_temperature)
  TEMPLATABLE_VALUE(float, target_temperature)
  TEMPLATABLE_VALUE(water_heater::WaterHeaterMode, mode)

  void play(const Ts &...x) override {
    if (this->current_temperature_.has_value()) {
      this->parent_->set_current_temperature(this->current_temperature_.value(x...));
    }
    bool needs_call = this->target_temperature_.has_value() || this->mode_.has_value();
    if (needs_call) {
      auto call = this->parent_->make_call();
      if (this->target_temperature_.has_value()) {
        call.set_target_temperature(this->target_temperature_.value(x...));
      }
      if (this->mode_.has_value()) {
        call.set_mode(this->mode_.value(x...));
      }
      call.perform();
    } else {
      this->parent_->publish_state();
    }
  }
};

}  // namespace esphome::template_
