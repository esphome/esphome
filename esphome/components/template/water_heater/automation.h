#pragma once

#include "template_water_heater.h"
#include "esphome/core/automation.h"

namespace esphome {
namespace template_ {

template<typename... Ts> class TemplateWaterHeaterPublishAction : public Action<Ts...>, public Parented<TemplateWaterHeater> {
 public:
  TEMPLATABLE_VALUE(float, current_temperature)
  TEMPLATABLE_VALUE(float, target_temperature)
  TEMPLATABLE_VALUE(water_heater::WaterHeaterMode, mode)

  void play(const Ts &...x) override {
    if (this->current_temperature_.has_value()) {
      this->parent_->current_temperature = this->current_temperature_.value(x...);
    }
    if (this->target_temperature_.has_value()) {
      this->parent_->target_temperature = this->target_temperature_.value(x...);
    }
    if (this->mode_.has_value()) {
      this->parent_->mode = this->mode_.value(x...);
    }
    this->parent_->publish_state();
  }
};

}  // namespace template_
}  // namespace esphome