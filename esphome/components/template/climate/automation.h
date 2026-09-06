#pragma once

#include "template_climate.h"
#include "esphome/core/automation.h"

namespace esphome::template_ {

template<typename... Ts>
class TemplateClimatePublishAction final : public Action<Ts...>, public Parented<TemplateClimate> {
 public:
  TEMPLATABLE_VALUE(float, current_temperature)
  TEMPLATABLE_VALUE(float, current_humidity)
  TEMPLATABLE_VALUE(float, target_temperature)
  TEMPLATABLE_VALUE(float, target_temperature_low)
  TEMPLATABLE_VALUE(float, target_temperature_high)
  TEMPLATABLE_VALUE(float, target_humidity)
  TEMPLATABLE_VALUE(climate::ClimateMode, mode)
  TEMPLATABLE_VALUE(climate::ClimateAction, action)
  TEMPLATABLE_VALUE(climate::ClimateFanMode, fan_mode)
  TEMPLATABLE_VALUE(std::string, custom_fan_mode)
  TEMPLATABLE_VALUE(climate::ClimateSwingMode, swing_mode)
  TEMPLATABLE_VALUE(climate::ClimatePreset, preset)
  TEMPLATABLE_VALUE(std::string, custom_preset)

  void play(const Ts &...x) override {
    if (this->current_temperature_.has_value())
      this->parent_->current_temperature = this->current_temperature_.value(x...);
    if (this->current_humidity_.has_value())
      this->parent_->current_humidity = this->current_humidity_.value(x...);
    if (this->target_temperature_.has_value())
      this->parent_->set_target_temperature(this->target_temperature_.value(x...));
    if (this->target_temperature_low_.has_value())
      this->parent_->set_target_temperature_low(this->target_temperature_low_.value(x...));
    if (this->target_temperature_high_.has_value())
      this->parent_->set_target_temperature_high(this->target_temperature_high_.value(x...));
    if (this->target_humidity_.has_value())
      this->parent_->set_target_humidity(this->target_humidity_.value(x...));
    if (this->mode_.has_value())
      this->parent_->set_mode(this->mode_.value(x...));
    if (this->action_.has_value())
      this->parent_->action = this->action_.value(x...);
    if (this->fan_mode_.has_value())
      this->parent_->set_fan_mode(this->fan_mode_.value(x...));
    if (this->custom_fan_mode_.has_value())
      this->parent_->set_custom_fan_mode(StringRef(this->custom_fan_mode_.value(x...)));
    if (this->swing_mode_.has_value())
      this->parent_->set_swing_mode(this->swing_mode_.value(x...));
    if (this->preset_.has_value())
      this->parent_->set_preset(this->preset_.value(x...));
    if (this->custom_preset_.has_value())
      this->parent_->set_custom_preset(StringRef(this->custom_preset_.value(x...)));

    this->parent_->publish_state();
  }
};

}  // namespace esphome::template_
