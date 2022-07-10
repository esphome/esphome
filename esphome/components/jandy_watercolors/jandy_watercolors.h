#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include "esphome/core/automation.h"

#include "esphome/components/output/binary_output.h"
#include "esphome/components/light/light_output.h"
#include "esphome/components/light/light_state.h"
#include "esphome/components/light/light_effect.h"

namespace esphome {
namespace jandy_watercolors {
static const char *const TAG = "jandy-watercolors";

static const int TOTAL_SUPPORTED_EFFECTS = 14;
static const int JANDY_UNKNOWN_EFFECT = -100;
static const int JANDY_FIRST_EFFECT = 0;
static const int JANDY_RESET_EFFECT = -1;

static const int NO_EFFECT_CHANGE_TIMESPAN = 7100;
static const int RESET_EFFECT_TIMESPAN = 5100;
static const int NEXT_EFFECT_MAX_TIMESPAN = 3100;
static const int NEXT_EFFECT_MIN_TIMESPAN = 1100;
static const int MINIMUM_LIGHT_ON_TIMESPAN = 1000;

class JandyWatercolorsLightOutput : public light::LightOutput, public esphome::Component {
 public:
  void loop() override;

  void set_target_effect(int target_effect);

  void target_next_effect();

  void reset();

  void setup_state(light::LightState *state) override;

  void set_output(output::BinaryOutput *output);

  light::LightTraits get_traits() override;

  void write_state(light::LightState *state) override;

 protected:
  void calculate_if_transition_();

  void update_active_effect_();

  void turn_on_output_();

  void turn_off_output_();

  std::string get_effect_name_from_id_(int effect_id);

  output::BinaryOutput *output_;
  light::LightState *light_state_;
  int current_effect_ = JANDY_UNKNOWN_EFFECT;
  int target_effect_ = JANDY_FIRST_EFFECT;
  int target_effect_after_reset_ = JANDY_RESET_EFFECT;
  bool is_output_on_ = false;
  bool is_light_on_ = false;
  uint32_t last_turned_off_ = 0;
  uint32_t last_turned_on_ = 0;

  std::string effect_names_[TOTAL_SUPPORTED_EFFECTS] = {
      "Alpine White", "Sky Blue",  "Cobalt Blue", "Caribbean Blue",    "Spring Green",      "Emerald Green",
      "Emerald Rose", "Magenta",   "Violet",      "Slow Color Splash", "Fast Color Splash", "America the Beautiful",
      "Fat Tuesday",  "Disco Tech"};
};

class JandyWatercolorsEffect : public light::LightEffect {
 public:
  explicit JandyWatercolorsEffect(JandyWatercolorsLightOutput *output, const std::string &name, int effect_position)
      : LightEffect(name) {
    this->output_ = output;
    this->effect_position_ = effect_position;
  }

  void apply() override { this->output_->set_target_effect(this->effect_position_); };

 protected:
  JandyWatercolorsLightOutput *output_{nullptr};
  int effect_position_ = 0;
};

template<typename... Ts> class JandyWatercolorsResetAction : public Action<Ts...> {
 public:
  JandyWatercolorsResetAction(JandyWatercolorsLightOutput *light_output) : light_output_(light_output) {}

  void play(Ts... x) override { this->light_output_->reset(); }

 protected:
  JandyWatercolorsLightOutput *light_output_;
};

template<typename... Ts> class JandyWatercolorsNextEffectAction : public Action<Ts...> {
 public:
  JandyWatercolorsNextEffectAction(JandyWatercolorsLightOutput *light_output) : light_output_(light_output) {}

  void play(Ts... x) override { this->light_output_->target_next_effect(); }

 protected:
  JandyWatercolorsLightOutput *light_output_;
};

}  // namespace jandy_watercolors
}  // namespace esphome
