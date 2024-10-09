#include <utility>
#include <vector>
#include <cinttypes>  // For PRIu32 an PRId32, portable formatting of values across platforms.

#include "esphome/core/automation.h"
#include "esphome/core/color.h"
#include "esphome/core/log.h"
#include "light_effect.h"
#include "light_state.h"
#include "color_mode.h"
#include "flame_light_effects.h"

/*
 * Original version written by Bill Adams (https://github.com/TikiBill).
 * If you find this useful or do any cool environments with it, please let him know
 * by emailing light-effects@lava-data.com.
 */

namespace esphome {
namespace light {
void FlameLightEffect::start() {
  ESP_LOGD("FlameLightEffect", "start()");
  this->is_baseline_brightness_needed_ = true;
  this->is_custom_color_set_needed_ = true;
  this->flickers_left_ = 0;
  if (this->have_custom_flicker_intensity_) {
    ESP_LOGD("FlameLightEffect", "start()    User supplied flicker intensity: %.3f    Overall intensity: %.3f",
             this->flicker_intensity_, this->intensity_);
  } else {
    this->flicker_intensity_ = this->intensity_ / this->number_levels_ / 2.0f;
    ESP_LOGD("FlameLightEffect", "start()    Calculated flicker intensity: %.3f    Overall intensity: %.3f",
             this->flicker_intensity_, this->intensity_);
  }

  ESP_LOGD("FlameLightEffect", "start()    Speed: %" PRIu32 "ms   Jitter: %" PRIu32, this->transition_length_ms_,
           this->transition_length_jitter_ms_);

  ESP_LOGD("FlameLightEffect", "start()    User supplied %d colors.", this->colors_.size());
  for (int i = 0; i < this->colors_.size(); i++) {
    ESP_LOGD("FlameLightEffect", "start()    Color %d    R: %d    G: %d    B: %d", i, this->colors_[i].red,
             this->colors_[i].green, this->colors_[i].blue);
  }

  if (this->flicker_level_probabilities_.empty()) {
    this->flicker_level_probabilities_ = {0.5f, 0.3f, 0.08f};
    ESP_LOGD("FlameLightEffect", "start()    Default flicker probability.");
  } else {
    ESP_LOGD("FlameLightEffect", "start()    User supplied %d flicker probabilities.",
             this->flicker_level_probabilities_.size());
  }

  this->number_levels_ = this->flicker_level_probabilities_.size();

  // Ensure the probability count matches the number of levels.
  while (this->flicker_level_probabilities_.size() < this->number_levels_) {
    float next_val = this->flicker_level_probabilities_.empty()
                         ? 1.0f
                         : this->flicker_level_probabilities_[this->flicker_level_probabilities_.size() - 1] / 2.0f;
    ESP_LOGW("FlameLightEffect", "start()    Not enough flicker_level_probability values, adding %.3f.", next_val);
    this->flicker_level_probabilities_.push_back(next_val);
  }

  float cumulative_probability = 0.0f;
  for (int i = 0; i < this->flicker_level_probabilities_.size(); i++) {
    cumulative_probability += this->flicker_level_probabilities_[i];
    ESP_LOGD("FlameLightEffect", "start()    Flicker Probability %d: %.2f", i, this->flicker_level_probabilities_[i]);
  }

  if (cumulative_probability >= 1.0f) {
    ESP_LOGW("FlameLightEffect",
             "start()    Your cumulative flicker probability is >= 100%% (total is %.1f%%) -- Zero non-flicker time.",
             cumulative_probability * 100.0f);
  } else {
    ESP_LOGD("FlameLightEffect", "start()   cumulative flicker probability: %.3f", cumulative_probability);
  }

  if (this->number_flickers_config_.empty()) {
    this->number_flickers_config_ = {
        FlameEffectNumberFlickers(0.40f, 2),  // The first probability does not matter, it acts as the fall-through.
        FlameEffectNumberFlickers(0.20f, 4), FlameEffectNumberFlickers(0.10f, 8), FlameEffectNumberFlickers(0.05f, 10),
        FlameEffectNumberFlickers(3, 1),  // At level 3, we force a single flicker.
    };
  }

  ESP_LOGD("FlameLightEffect", "start()    Done.");
}

void FlameLightEffect::apply() {
  if (this->state_->is_transformer_active()) {
    // Something is already running.
    return;
  }

  if (this->is_custom_color_set_needed_ && !this->colors_.empty()) {
    // Setting the custom color is not in the start() because we
    // do not want to interrupt a running transformer.

    this->is_custom_color_set_needed_ = false;  // Only set the initial color once.

    // Logging colors as integers because it is easier to take the numbers
    // to other tools (such as HTML color pickers) for comparison.
    ESP_LOGD("FlameLightEffect", "Setting initial color to color @ idx 0:  R: %d  G: %d  B: %d  (W: %d)",
             this->colors_[0].red, this->colors_[0].green, this->colors_[0].blue, this->colors_[0].white);

    this->initial_brightness_ = 1.0f;
    this->baseline_brightness_ = this->is_baseline_brightness_dim_ ? 1.0f - this->intensity_ : 1.0f;
    this->color_mode_ = ColorMode::RGB;
    set_min_max_brightness();
    // We determined everything here so no need to get it again.
    this->is_baseline_brightness_needed_ = false;

    auto call = this->state_->make_call();
    call.set_publish(false);
    call.set_save(false);
    call.set_color_mode(this->color_mode_);
    // Commented out 'cause We will use the default transistion.
    // call.set_transition_length(this->transition_length_ms_ * 4);
    call.set_brightness(baseline_brightness_);
    call.set_rgb(this->colors_[0].red / 255.0f, this->colors_[0].green / 255.0f, this->colors_[0].blue / 255.0f);
    call.set_state(true);
    call.perform();

    return;
  }

  if (this->is_baseline_brightness_needed_) {
    // This is only needed if we are not using a custom color.
    this->is_baseline_brightness_needed_ = false;
    this->initial_brightness_ = this->baseline_brightness_ = this->state_->current_values.get_brightness();
    this->color_mode_ = this->state_->current_values.get_color_mode();

    ESP_LOGD("FlameLightEffect", "Initial/Baseline Brightness: %.3f   Intensity: %.3f   Flicker Intensity: %.3f",
             this->baseline_brightness_, this->intensity_, this->flicker_intensity_);

    if (this->is_baseline_brightness_dim_) {
      // Special case to ensure our max brightness increase can be accommodated. E.g
      // if the bulb is at 100% and we want to do a normally-dim fireplace, reduce the
      // brightness to accommodate.
      if (this->baseline_brightness_ >= 1.0f - this->intensity_) {
        this->baseline_brightness_ = 1.0f - this->intensity_;
        ESP_LOGD("FlameLightEffect", "Adjusting the baseline brightness to %.3f", this->baseline_brightness_);
        auto call = this->state_->make_call();
        call.set_publish(false);
        call.set_save(false);
        call.set_brightness(this->baseline_brightness_);
        call.set_state(true);
        call.perform();
      }
    }

    set_min_max_brightness();
    return;  // Important! Wait for the next pass to start effects since we may have stared a transition.
  }

  float new_brightness;
  uint32_t transition_length_ms;

  if (this->flickers_left_ > 0) {
    // We decrement the counter below.
    if (this->probability_true_(0.5f)) {
      transition_length_ms = this->transition_length_ms_;
    } else {
      transition_length_ms = this->transition_length_ms_ + this->transition_length_jitter_ms_;
    }

    if (this->is_in_bright_flicker_state_) {
      new_brightness = this->flicker_dim_brightness_;
      this->is_in_bright_flicker_state_ = false;
    } else {
      new_brightness = this->flicker_bright_brightness_;
      this->is_in_bright_flicker_state_ = true;
    }
  } else {
    float r = random_float();

    // If we do not find a match, then use these defaults:
    float brightness_sublevel = 0.0f;
    this->flicker_state_ = 0;

    float cumulative_probability = 0.0f;
    for (int i = this->flicker_level_probabilities_.size(); i > 0; i--) {
      cumulative_probability += this->flicker_level_probabilities_[i - 1];
      if (r <= cumulative_probability) {
        brightness_sublevel = i;
        this->flicker_state_ = i;
        break;
      }
    }

    this->set_flicker_brightness_levels(brightness_sublevel);
    this->clamp_flicker_brightness_levels_();

    this->flickers_left_ = this->determine_number_flickers_();
    transition_length_ms = this->determine_transistion_length_for_new_state_();
    this->is_in_bright_flicker_state_ = this->probability_true_(0.5f);
    new_brightness =
        this->is_in_bright_flicker_state_ ? this->flicker_bright_brightness_ : this->flicker_dim_brightness_;
    this->previous_flicker_state_ = this->flicker_state_;

    ESP_LOGD("FlameLightEffect",
             "Random Value: %.3f  ->  Level: %.1f    Flicker State: %" PRIu32
             "    Flicker Dim: %.3f    Bright: %.3f    "
             "Flicker Count: %" PRIu32,
             r, brightness_sublevel, this->flicker_state_, this->flicker_dim_brightness_,
             this->flicker_bright_brightness_, this->flickers_left_);
  }

  if (transition_length_ms < this->transition_length_ms_) {
    ESP_LOGW("FlameLightEffect", "Oops...the transition length is %" PRIu32 "ms, clamping to %" PRIu32 "ms",
             transition_length_ms, this->transition_length_ms_);
    transition_length_ms = this->transition_length_ms_;
  }

  this->flickers_left_ -= 1;
  //   ESP_LOGD("FlameLightEffect", "Brightness: %.3f    Transition Length: %" PRId32 "ms    Short Flickers Left: %d",
  //     new_brightness, transition_length_ms, flickers_left_);
  auto call = this->state_->make_call();
  call.set_publish(false);
  call.set_save(false);
  call.set_color_mode(this->color_mode_);

  if (this->colors_.size() < 2) {
    // Nothing to do.
  } else if (this->colors_.size() == 2) {
    // Two colors. Auto-determine the gradient value.
    float color_fade_amount;
    if (this->is_baseline_brightness_dim_) {
      color_fade_amount = (new_brightness - this->min_brightness_) / (this->max_brightness_ - this->min_brightness_);
    } else {
      color_fade_amount = (this->max_brightness_ - new_brightness) / (this->max_brightness_ - this->min_brightness_);
    }

    if (this->use_exponential_gradient_) {
      color_fade_amount = std::pow(10.0f, color_fade_amount) / 10.0f;
    }

    uint8_t amt = color_fade_amount >= 1.0f ? 255 : color_fade_amount <= 0.0f ? 0 : (color_fade_amount * 255.0f);

    Color c = this->colors_[0].gradient(this->colors_[1], amt);
    ESP_LOGD("FlameLightEffect", "Color Fade: %.1f%% -> %d    R: %d    G: %d    B: %d", color_fade_amount * 100.0f, amt,
             c.red, c.green, c.blue);

    call.set_rgb(c.red / 255.0f, c.green / 255.0f, c.blue / 255.0f);
  } else {
    // Assume a color per level. If there are not enough colors, use the last one.
    Color c;
    if (this->flicker_state_ < this->colors_.size()) {
      c = this->colors_[this->flicker_state_];
    } else {
      c = this->colors_[this->colors_.size() - 1];
    }
    ESP_LOGD("FlameLightEffect", "State %" PRIu32 " Color:    R: %d    G: %d    B: %d", this->flicker_state_, c.red,
             c.green, c.blue);

    call.set_rgb(c.red / 255.0f, c.green / 255.0f, c.blue / 255.0f);
  }

  call.set_transition_length(transition_length_ms);
  call.set_brightness(new_brightness);

  call.perform();
}

/*
 * Determine the number of flickers for the new state.
 */
uint32_t FlameLightEffect::determine_number_flickers_() {
  if (this->number_flickers_config_.empty()) {
    ESP_LOGW("FlameLightEffect", "No number of flickers config at all?");
    return 4;
  }

  for (auto &flicker_config : this->number_flickers_config_) {
    if (this->flicker_state_ > 0 && this->flicker_state_ == flicker_config.force_at_level) {
      return flicker_config.number_flickers;
    }
  }

  float r = random_float();
  float cumulative_probability = 0.0f;
  for (int i = this->number_flickers_config_.size() - 1; i >= 0; i--) {
    if (this->number_flickers_config_[i].probability <= 0.0f) {
      continue;
    }

    cumulative_probability += number_flickers_config_[i].probability;
    if (r <= cumulative_probability) {
      return number_flickers_config_[i].number_flickers;
    }
  }

  // Fall through, use the first item.
  return number_flickers_config_[0].number_flickers;
}

uint32_t FlameLightEffect::determine_transistion_length_for_new_state_() {
  uint32_t transition_length_ms;
  uint32_t delta = abs((int) this->flicker_state_ - (int) this->previous_flicker_state_);

  if (this->probability_true_(0.5)) {
    transition_length_ms = this->transition_length_ms_ * (delta < 1 ? 1 : delta);
  } else {
    transition_length_ms = (this->transition_length_ms_ + this->transition_length_jitter_ms_) * (delta < 1 ? 1 : delta);
  }

  if (transition_length_ms < this->transition_length_ms_) {
    ESP_LOGW("FlameLightEffect", "Oops...the transition length is %" PRId32 "ms, clamping to %" PRId32 "ms",
             transition_length_ms, this->transition_length_ms_);

    transition_length_ms = this->transition_length_ms_;
  }

  return transition_length_ms;
}

/* *************** Candle *************** */
void CandleLightEffect::set_flicker_brightness_levels(float level) {
  if (this->flicker_state_ == 0) {
    // No flicker.
    this->flicker_bright_brightness_ = this->baseline_brightness_;
    this->flicker_dim_brightness_ = this->baseline_brightness_;
  } else {
    this->flicker_bright_brightness_ =
        this->baseline_brightness_ - (level * this->intensity_ / this->number_levels_ * this->initial_brightness_);
    this->flicker_dim_brightness_ =
        this->flicker_bright_brightness_ - (this->flicker_intensity_ * this->initial_brightness_);
  }
}

void CandleLightEffect::set_min_max_brightness() {
  this->max_brightness_ = this->baseline_brightness_;
  this->min_brightness_ = this->baseline_brightness_ - (this->intensity_ * this->initial_brightness_);

  ESP_LOGD("CandleLightEffect", "Min Brightness: %.3f    Max Brightness: %.3f", this->min_brightness_,
           this->max_brightness_);
}

/* *************** Fireplace *************** */
void FireplaceLightEffect::start() {
  ESP_LOGW("FireplaceLightEffect", "Setting is_baseline_brightness_dim_ to true");
  this->is_baseline_brightness_dim_ = true;
  FlameLightEffect::start();
}

void FireplaceLightEffect::set_flicker_brightness_levels(float level) {
  if (this->flicker_state_ == 0) {
    // No flicker.
    this->flicker_bright_brightness_ = this->baseline_brightness_;
    this->flicker_dim_brightness_ = this->baseline_brightness_;
  } else {
    this->flicker_dim_brightness_ =
        this->baseline_brightness_ * (1 + (level * this->intensity_ / this->number_levels_));
    this->flicker_bright_brightness_ = this->flicker_dim_brightness_ + (this->flicker_intensity_);
  }
}

void FireplaceLightEffect::set_min_max_brightness() {
  this->min_brightness_ = this->baseline_brightness_;
  this->max_brightness_ = this->min_brightness_ + (this->intensity_ * this->initial_brightness_);

  ESP_LOGD("FireplaceLightEffect", "Min Brightness: %.3f    Max Brightness: %.3f", this->min_brightness_,
           this->max_brightness_);
}
}  // namespace light
};  // namespace esphome
