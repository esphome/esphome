#pragma once

#include "esphome/core/automation.h"
#include "esphome/core/helpers.h"

#include "dfrobot_sen0395.h"

namespace esphome::dfrobot_sen0395 {

template<typename... Ts>
class DfrobotSen0395SettingsAction final : public Action<Ts...>, public Parented<DfrobotSen0395Component> {
 public:
  TEMPLATABLE_VALUE(int8_t, factory_reset)
  TEMPLATABLE_VALUE(int8_t, start_after_power_on)
  TEMPLATABLE_VALUE(int8_t, turn_on_led)
  TEMPLATABLE_VALUE(int8_t, presence_via_uart)
  TEMPLATABLE_VALUE(int8_t, sensitivity)
  TEMPLATABLE_VALUE(float, delay_after_detect)
  TEMPLATABLE_VALUE(float, delay_after_disappear)
  TEMPLATABLE_VALUE(float, det_min1)
  TEMPLATABLE_VALUE(float, det_max1)
  TEMPLATABLE_VALUE(float, det_min2)
  TEMPLATABLE_VALUE(float, det_max2)
  TEMPLATABLE_VALUE(float, det_min3)
  TEMPLATABLE_VALUE(float, det_max3)
  TEMPLATABLE_VALUE(float, det_min4)
  TEMPLATABLE_VALUE(float, det_max4)

  void play(const Ts &...x) override {
    this->parent_->enqueue(CommandType::COMMAND_TYPE_POWER, false);
    if (this->factory_reset_.has_value() && this->factory_reset_.value(x...) == true) {
      this->parent_->enqueue(CommandType::COMMAND_TYPE_FACTORY_RESET);
    }
    if (this->det_min1_.has_value() && this->det_max1_.has_value()) {
      // Each segment is read once, so the guard and the command see the same values.
      const float min1 = this->det_min1_.value(x...);
      if (min1 >= 0) {
        const float max1 = this->det_max1_.value(x...);
        if (max1 >= 0) {
          this->parent_->enqueue_det_range_cfg(min1, max1, this->det_min2_.value_or(x..., -1),
                                               this->det_max2_.value_or(x..., -1), this->det_min3_.value_or(x..., -1),
                                               this->det_max3_.value_or(x..., -1), this->det_min4_.value_or(x..., -1),
                                               this->det_max4_.value_or(x..., -1));
        }
      }
    }
    if (this->delay_after_detect_.has_value() && this->delay_after_disappear_.has_value()) {
      float detect = this->delay_after_detect_.value(x...);
      float disappear = this->delay_after_disappear_.value(x...);
      if (detect >= 0 && disappear >= 0) {
        this->parent_->enqueue_set_latency(detect, disappear);
      }
    }
    if (this->start_after_power_on_.has_value()) {
      int8_t val = this->start_after_power_on_.value(x...);
      if (val >= 0) {
        this->parent_->enqueue(CommandType::COMMAND_TYPE_SENSOR_CFG_START, val);
      }
    }
    if (this->turn_on_led_.has_value()) {
      int8_t val = this->turn_on_led_.value(x...);
      if (val >= 0) {
        this->parent_->enqueue(CommandType::COMMAND_TYPE_LED_MODE, val);
      }
    }
    if (this->presence_via_uart_.has_value()) {
      int8_t val = this->presence_via_uart_.value(x...);
      if (val >= 0) {
        this->parent_->enqueue(CommandType::COMMAND_TYPE_UART_OUTPUT, val);
      }
    }
    if (this->sensitivity_.has_value()) {
      int8_t val = this->sensitivity_.value(x...);
      if (val >= 0) {
        this->parent_->enqueue(CommandType::COMMAND_TYPE_SENSITIVITY, val);
      }
    }
    this->parent_->enqueue(CommandType::COMMAND_TYPE_SAVE_CFG);
    this->parent_->enqueue(CommandType::COMMAND_TYPE_POWER, true);
  }
};

}  // namespace esphome::dfrobot_sen0395
