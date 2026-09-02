#pragma once

#include "esphome/core/automation.h"
#include "tas2780.h"

namespace esphome::tas2780 {

template<typename... Ts> class ResetAction : public Action<Ts...>, public Parented<TAS2780> {
 public:
  void play(const Ts &...x) override { this->parent_->reset(); }
};

template<typename... Ts> class ActivateAction : public Action<Ts...>, public Parented<TAS2780> {
 public:
  TEMPLATABLE_VALUE(uint8_t, power_mode)

  void play(const Ts &...x) override {
    if (this->power_mode_.has_value()) {
      this->parent_->activate(this->power_mode_.value(x...));
    } else {
      this->parent_->activate();
    }
  }
};

template<typename... Ts> class UpdateConfigAction : public Action<Ts...>, public Parented<TAS2780> {
 public:
  TEMPLATABLE_VALUE(uint8_t, amp_level)
  TEMPLATABLE_VALUE(float, vol_range_min)
  TEMPLATABLE_VALUE(float, vol_range_max)
  TEMPLATABLE_VALUE(ChannelSelect, channel)

  void play(const Ts &...x) override {
    if (this->amp_level_.has_value()) {
      this->parent_->set_amp_level(this->amp_level_.value(x...));
    }
    if (this->vol_range_min_.has_value()) {
      this->parent_->set_vol_range_min(this->vol_range_min_.value(x...));
    }
    if (this->vol_range_max_.has_value()) {
      this->parent_->set_vol_range_max(this->vol_range_max_.value(x...));
    }
    if (this->channel_.has_value()) {
      this->parent_->set_selected_channel(this->channel_.value(x...));
    }
    if (this->amp_level_.has_value() || this->channel_.has_value()) {
      this->parent_->apply_amp_and_channel_config();
    }
    if (this->vol_range_min_.has_value() || this->vol_range_max_.has_value()) {
      this->parent_->set_volume(this->parent_->volume());
    }
  }
};

template<typename... Ts> class DeactivateAction : public Action<Ts...>, public Parented<TAS2780> {
 public:
  void play(const Ts &...x) override { this->parent_->deactivate(); }
};

}  // namespace esphome::tas2780
