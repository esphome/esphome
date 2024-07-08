#pragma once

#include <utility>
#include <vector>

#include "esphome/core/automation.h"
#include "esphome/core/color.h"
#include "light_effect.h"
#include "light_state.h"
#include "color_mode.h"

/*
 * Original version written by Bill Adams (https://github.com/TikiBill).
 * If you find this useful or do any cool environments with it, please let him know
 * by emailing light-effects@lava-data.com.
 */

namespace esphome {
namespace light {

struct FlameEffectNumberFlickers {
  uint32_t force_at_level = 0;
  float probability = 0.1f;
  uint32_t number_flickers = 5;

  // Use int rather than unit32_t otherwise the compiler thinks the calls are ambiguous.
  FlameEffectNumberFlickers() : force_at_level(0), probability(0.1f), number_flickers(5) {}
  FlameEffectNumberFlickers(float probability, int number_flickers)
      : force_at_level(0), probability(probability), number_flickers(number_flickers) {}
  FlameEffectNumberFlickers(int force_at_level, float probability, int number_flickers)
      : force_at_level(force_at_level), probability(probability), number_flickers(number_flickers) {}
  FlameEffectNumberFlickers(int force_at_level, int number_flickers)
      : force_at_level(force_at_level), probability(0.0f), number_flickers(number_flickers) {}
};

class FlameLightEffect : public LightEffect {
 public:
  explicit FlameLightEffect(const std::string &name) : LightEffect(name) {}

  void start() override;
  void apply() override;

  /*
   * The overall max intensity swing of the flicker, based on the starting brightness.
   */
  void set_intensity(float intensity) {
    if (intensity > 0.0f) {
      this->intensity_ = intensity;
    }
  }

  void set_flicker_intensity(float flicker_intensity) {
    if (flicker_intensity > 0.0f) {
      this->flicker_intensity_ = flicker_intensity;
      this->have_custom_flicker_intensity_ = true;
    }
  }

  void set_flicker_transition_length_ms(uint32_t length_ms) {
    if (length_ms > 0) {
      this->transition_length_ms_ = length_ms;
    }
  }

  void set_flicker_transition_length_ms_jitter(uint32_t length_ms) {
    if (length_ms > 0) {
      this->transition_length_jitter_ms_ = length_ms;
    }
  }

  void set_use_exponential_gradient(bool enabled) { this->use_exponential_gradient_ = enabled; }

  void set_colors(const std::vector<Color> &colors) {
    if (!colors.empty()) {
      this->colors_ = colors;
      this->is_custom_color_set_needed_ = true;
    }
  }

  void set_flicker_level_probabilities(const std::vector<float> &values) {
    if (!values.empty()) {
      this->flicker_level_probabilities_ = values;
    }
  }

  void set_number_flickers_config(const std::vector<FlameEffectNumberFlickers> &config) {
    if (!config.empty()) {
      this->number_flickers_config_ = config;
    }
  }

 protected:
  /*
   * The overall brightness swing of all levels of flicker.
   */
  float intensity_ = 0.15f;

  /*
   * The intensity of a high/low flicker, usually the overall intensity / number levels / 2.
   */
  float flicker_intensity_;
  bool have_custom_flicker_intensity_ = false;

  uint32_t transition_length_ms_ = 100;
  uint32_t transition_length_jitter_ms_ = 10;

  std::vector<float> flicker_level_probabilities_ = {};  //{ 0.8f, 0.4f, 0.08f };

  std::vector<FlameEffectNumberFlickers> number_flickers_config_ = {};

  bool use_exponential_gradient_ = true;

  float number_levels_ = 3.0f;

  bool is_custom_color_set_needed_ = false;
  std::vector<Color> colors_;

  bool is_baseline_brightness_dim_ = false;

  ColorMode color_mode_ = ColorMode::UNKNOWN;

  // State
  float initial_brightness_;
  bool is_baseline_brightness_needed_;
  float baseline_brightness_;
  uint32_t flicker_state_ = 0;
  uint32_t previous_flicker_state_ = 0;
  uint32_t flickers_left_ = 0;

  /*
   * Track if the flicker is on the bright or dim part of the
   * cycle so it can be toggled.
   */
  bool is_in_bright_flicker_state_ = true;

  /*
   * The brightness percent for the bright part of the flicker.
   * This gets changes in the code when a new flicker state is determined
   * and is scaled based on many factors.
   */
  float flicker_bright_brightness_ = 0.95f;

  /*
   * The brightness percent for the dim part of the flicker.
   * This gets changes in the code when a new flicker state is determined
   * and is scaled based on many factors.
   */
  float flicker_dim_brightness_ = 0.90f;

  // float brightness_scale_ = 1.0f;

  virtual void set_flicker_brightness_levels(float level) = 0;

  float max_brightness_ = 1.0f;
  float min_brightness_ = 0.0f;
  /*
   * Called after the baseline brightness is found. The
   * method should set max_brightness_ and min_brightness_
   * which are in turn used to determine the color of light
   * in a two-color mode based on the brightness.
   */
  virtual void set_min_max_brightness() = 0;

  /* Ensure the flicker brightness levels are not above 100% or below 0%.*/
  void clamp_flicker_brightness_levels_() {
    if (this->flicker_bright_brightness_ > 1.0f) {
      this->flicker_bright_brightness_ = 1.0f;
    } else if (this->flicker_bright_brightness_ < 0.0f) {
      this->flicker_bright_brightness_ = 0.0f;
    }

    if (this->flicker_dim_brightness_ > 1.0f) {
      this->flicker_dim_brightness_ = 1.0f;
    } else if (this->flicker_dim_brightness_ < 0.0f) {
      this->flicker_dim_brightness_ = 0.0f;
    }
  }

  bool probability_true_(float percentage) { return percentage <= random_float(); }

  /*
   * Determine the number of flickers for the new state.
   */
  uint32_t determine_number_flickers_();
  uint32_t determine_transistion_length_for_new_state_();
};

/*
 * A candle has a normal brightness and flickers dim when a breese comes by.
 */
class CandleLightEffect : public FlameLightEffect {
 public:
  explicit CandleLightEffect(const std::string &name) : FlameLightEffect(name) {}

 protected:
  void set_flicker_brightness_levels(float level) override;
  void set_min_max_brightness() override;
};

/*
 * A fireplace is glowing with embers with occasional flames brightening the room.
 * Basically the opposite of a candle.
 */
class FireplaceLightEffect : public FlameLightEffect {
 public:
  explicit FireplaceLightEffect(const std::string &name) : FlameLightEffect(name) {}

  void start() override;

 protected:
  void set_flicker_brightness_levels(float level) override;
  void set_min_max_brightness() override;
};

}  // namespace light
}  // namespace esphome
