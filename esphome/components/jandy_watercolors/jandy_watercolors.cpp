#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include "esphome/components/output/binary_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_effect.h"
#include "jandy_watercolors.h"

namespace esphome {
namespace jandy_watercolors {
void JandyWatercolorsLightOutput::setup_state(light::LightState *state) {
  this->light_state_ = state;

  for (int i = 0; i < TOTAL_SUPPORTED_EFFECTS; i++) {
    state->add_effects({new JandyWatercolorsEffect(this, this->effect_names_[i], i)});
  }
}

void JandyWatercolorsLightOutput::loop() {
  // If current_effect == UNKNOWN, then the device was restarted and we actually don't know
  // how long has the light been off. We will reset the light by setting the target effect
  // to RESET. This will only happen when the light is on.
  if (this->current_effect_ == JANDY_UNKNOWN_EFFECT && this->target_effect_ != JANDY_RESET_EFFECT) {
    this->target_effect_ = JANDY_RESET_EFFECT;
  }

  if (this->is_light_on_ && this->current_effect_ != this->target_effect_) {
    const uint32_t now = millis();

    if (this->is_output_on_) {
      // Light is on, so we need to wait for at least a second before turning it off
      auto last_on_duration = now - this->last_turned_on_;

      if (last_on_duration > MINIMUM_LIGHT_ON_TIMESPAN) {
        // We can turn the light off
        this->turn_off_output_();
      }
    } else {
      auto last_off_duration = now - this->last_turned_off_;
      if (this->target_effect_ == JANDY_RESET_EFFECT && last_off_duration >= RESET_EFFECT_TIMESPAN) {
        ESP_LOGV(TAG, "Turning On to Reset Light");
        this->target_effect_ = JANDY_FIRST_EFFECT;
        this->turn_on_output_();
      } else if (this->target_effect_ >= JANDY_FIRST_EFFECT && last_off_duration >= NEXT_EFFECT_MIN_TIMESPAN &&
                 last_off_duration <= NEXT_EFFECT_MAX_TIMESPAN) {
        ESP_LOGV(TAG, "Turning On to Next Effect");
        this->turn_on_output_();
      }
    }
  }
}

void JandyWatercolorsLightOutput::set_target_effect(int target_effect) {
  if (this->target_effect_ == target_effect ||
      (this->target_effect_ == JANDY_RESET_EFFECT && this->target_effect_after_reset_ == target_effect)) {
    return;
  }

  if (this->current_effect_ == JANDY_UNKNOWN_EFFECT &&
      (this->target_effect_ != JANDY_RESET_EFFECT || this->target_effect_after_reset_ != target_effect)) {
    this->target_effect_ = JANDY_RESET_EFFECT;
    this->target_effect_after_reset_ = target_effect;
    ESP_LOGD(TAG, "Resetting the light and setting next Effect to: '%s'",
             this->get_effect_name_from_id_(this->target_effect_).c_str());
  } else {
    this->target_effect_ = target_effect;
    ESP_LOGD(TAG, "Target Effect set to: '%s'", this->get_effect_name_from_id_(this->target_effect_).c_str());
  }
}

void JandyWatercolorsLightOutput::target_next_effect() {
  auto new_target_effect = this->target_effect_ + 1;
  if (new_target_effect >= TOTAL_SUPPORTED_EFFECTS || new_target_effect < JANDY_FIRST_EFFECT) {
    new_target_effect = JANDY_FIRST_EFFECT;
  }

  auto call = this->light_state_->turn_on();
  call.set_effect(this->get_effect_name_from_id_(new_target_effect));
  call.perform();

  this->target_effect_ = new_target_effect;
}

void JandyWatercolorsLightOutput::reset() {
  auto call = this->light_state_->turn_on();
  call.set_effect("None");
  call.perform();

  this->current_effect_ = JANDY_UNKNOWN_EFFECT;
}

void JandyWatercolorsLightOutput::set_output(output::BinaryOutput *output) { this->output_ = output; }

light::LightTraits JandyWatercolorsLightOutput::get_traits() {
  auto traits = light::LightTraits();
  traits.set_supported_color_modes({light::ColorMode::ON_OFF});
  return traits;
}

void JandyWatercolorsLightOutput::write_state(light::LightState *state) {
  bool should_be_on;
  state->current_values_as_binary(&should_be_on);
  if (should_be_on && !this->is_output_on_) {
    this->turn_on_output_();
    this->is_light_on_ = true;
    this->target_effect_ = this->current_effect_;
  } else if (!should_be_on && this->is_output_on_) {
    this->turn_off_output_();
    this->is_light_on_ = false;
  }

  this->update_active_effect_();
}

void JandyWatercolorsLightOutput::calculate_if_transition_() {
  if (this->is_output_on_) {
    const uint32_t now = millis();

    auto last_off_duration = now - this->last_turned_off_;

    // If last_turned_off is zero, then we don't know when the light was turned off.
    // This means that we cannot reliable determine what would happen when the light
    // turns on. Another part of the code (inside loop()) will take care of resetting
    // light.
    if (this->last_turned_off_ == 0) {
      last_off_duration = NO_EFFECT_CHANGE_TIMESPAN;
    }

    ESP_LOGV(TAG, "Last Off Duration '%d'", last_off_duration);
    ESP_LOGV(TAG, "Last Turned Off '%d'", this->last_turned_off_);

    if (last_off_duration >= NO_EFFECT_CHANGE_TIMESPAN) {
      // Do nothing, the light didn't move
    } else if (last_off_duration >= RESET_EFFECT_TIMESPAN) {
      // The light was reset, new effect is zero
      this->current_effect_ = JANDY_FIRST_EFFECT;
      // If we have a target effect after reset, apply it.
      if (this->target_effect_after_reset_ != JANDY_RESET_EFFECT) {
        this->target_effect_ = this->target_effect_after_reset_;
        this->target_effect_after_reset_ = JANDY_RESET_EFFECT;
      }
    } else if (last_off_duration >= NEXT_EFFECT_MIN_TIMESPAN && last_off_duration <= NEXT_EFFECT_MAX_TIMESPAN) {
      // Since we turned it on within the 3 second window, the effect moved
      this->current_effect_++;
      if (this->current_effect_ >= TOTAL_SUPPORTED_EFFECTS) {
        this->current_effect_ = JANDY_FIRST_EFFECT;
      }
    }
    this->update_active_effect_();
  }

  ESP_LOGD(TAG, "Effects - Active '%s' - Target: '%s'", this->get_effect_name_from_id_(this->current_effect_).c_str(),
           this->get_effect_name_from_id_(this->target_effect_).c_str());
}

void JandyWatercolorsLightOutput::update_active_effect_() {
  if (this->is_light_on_ && this->current_effect_ == this->target_effect_ &&
      this->target_effect_ >= JANDY_FIRST_EFFECT &&
      this->light_state_->get_effect_name() != this->get_effect_name_from_id_(this->current_effect_)) {
    ESP_LOGD(TAG, "Updating Active Effect on UI to: %s", this->get_effect_name_from_id_(this->current_effect_).c_str());

    auto call = this->light_state_->turn_on();
    call.set_effect(this->get_effect_name_from_id_(this->current_effect_));
    call.perform();
  }
}

void JandyWatercolorsLightOutput::turn_on_output_() {
  this->last_turned_on_ = millis();
  this->output_->turn_on();
  this->is_output_on_ = true;
  this->calculate_if_transition_();
}

void JandyWatercolorsLightOutput::turn_off_output_() {
  this->last_turned_off_ = millis();
  this->output_->turn_off();
  this->is_output_on_ = false;
}

std::string JandyWatercolorsLightOutput::get_effect_name_from_id_(int effect_id) {
  if (effect_id == JANDY_RESET_EFFECT) {
    return "Reset";
  }

  if (effect_id == JANDY_UNKNOWN_EFFECT) {
    return "Unknown";
  }

  if (effect_id < 0 || effect_id >= TOTAL_SUPPORTED_EFFECTS) {
    return "Invalid Effect ID";
  } else {
    return this->effect_names_[effect_id];
  }
}

}  // namespace jandy_watercolors
}  // namespace esphome
