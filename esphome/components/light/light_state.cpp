#include "light_state.h"
#include "esp_color_correction.h"
#include "esphome/core/defines.h"
#ifdef USE_LIGHT_TRANSITION_PUBLISH_INTERVAL
#include "esphome/core/application.h"
#endif
#include "esphome/core/controller_registry.h"
#include "esphome/core/log.h"
#include "light_output.h"
#include "transformers.h"

#include <limits>

namespace esphome::light {

static const char *const TAG = "light";

// Colour modes are bitmasks of capabilities. A mode the light doesn't support may be a bare set of
// required capabilities (see restore_state.py's colour mode inference): use the first supported
// mode that provides all of them, or leave it unchanged if there is none.
static ColorMode resolve_color_mode(const LightTraits &traits, ColorMode requested) {
  if (requested == ColorMode::UNKNOWN || traits.supports_color_mode(requested))
    return requested;
  auto wanted = static_cast<uint8_t>(requested);
  for (ColorMode mode : traits.get_supported_color_modes()) {
    if ((static_cast<uint8_t>(mode) & wanted) == wanted)
      return mode;
  }
  return requested;
}

LightState::LightState(LightOutput *output) : output_(output) {}

LightTraits LightState::get_traits() { return this->output_->get_traits(); }
LightCall LightState::turn_on() { return this->make_call().set_state(true); }
LightCall LightState::turn_off() { return this->make_call().set_state(false); }
LightCall LightState::toggle() { return this->make_call().set_state(!this->remote_values.is_on()); }
LightCall LightState::make_call() { return LightCall(this); }

void LightState::setup() {
  this->output_->setup_state(this);
  for (auto *effect : this->effects_) {
    effect->init_internal(this);
  }

  // Start with loop disabled if idle - respects any effects/transitions set up during initialization
  this->disable_loop_if_idle_();

  // When supported color temperature range is known, initialize color temperature setting within bounds.
  auto traits = this->get_traits();
  float min_mireds = traits.get_min_mireds();
  if (min_mireds > 0) {
    this->remote_values.set_color_temperature(min_mireds);
    this->current_values.set_color_temperature(min_mireds);
  }

  auto call = this->make_call();
  LightStateRTCState recovered{};
  bool restored = false;
  if (this->save_enabled_) {
    this->rtc_ = this->make_entity_preference<LightStateRTCState>();
    restored = this->rtc_.load(&recovered);
  }
  if (this->state_callback_) {
    this->state_callback_(recovered, restored);
    this->state_callback_ = nullptr;  // One-shot — no longer needed
  }

  // A light coming up on boot must never end up on-but-invisible: if the resolved restore
  // state is on but its brightness is zero (e.g. a stale/persisted value from before a
  // forced-on restore mode, or an inverted restore flipping a dim-to-0 off state to on),
  // reset it to full brightness.
  if (recovered.state && recovered.brightness == 0.0f) {
    recovered.brightness = 1.0f;
  }

  call.set_color_mode_if_supported(resolve_color_mode(traits, recovered.color_mode));
  call.set_state(recovered.state);
  call.set_brightness_if_supported(recovered.brightness);
  call.set_color_brightness_if_supported(recovered.color_brightness);
  call.set_red_if_supported(recovered.red);
  call.set_green_if_supported(recovered.green);
  call.set_blue_if_supported(recovered.blue);
  call.set_white_if_supported(recovered.white);
  call.set_color_temperature_if_supported(recovered.color_temp);
  call.set_cold_white_if_supported(recovered.cold_white);
  call.set_warm_white_if_supported(recovered.warm_white);
  if (recovered.effect != 0) {
    call.set_effect(recovered.effect);
  } else {
    call.set_transition_length_if_supported(0);
  }
  call.perform();
}
void LightState::dump_config() {
  ESP_LOGCONFIG(TAG, "Light '%s'", this->get_name().c_str());
  auto traits = this->get_traits();
  if (traits.supports_color_capability(ColorCapability::BRIGHTNESS)) {
#ifdef USE_LIGHT_GAMMA_LUT
    // Read the stored gamma * 100 directly so dump_config does not pull in get_gamma_correct()
    const unsigned gamma_x100 =
        this->gamma_table_ != nullptr ? progmem_read_uint16(&this->gamma_table_->gamma_x100) : 0;
#else
    const unsigned gamma_x100 = 0;
#endif
    ESP_LOGCONFIG(TAG,
                  "  Default Transition Length: %.1fs\n"
                  "  Gamma Correct: %u.%02u",
                  this->default_transition_length_ / 1e3f, gamma_x100 / 100, gamma_x100 % 100);
#ifdef USE_LIGHT_TRANSITION_PUBLISH_INTERVAL
    // The define is build wide; only lights that set the option have an interval
    if (this->transition_state_publish_interval_ != 0) {
      ESP_LOGCONFIG(TAG, "  Transition State Publish Interval: %" PRIu32 "ms",
                    this->transition_state_publish_interval_);
    }
#endif
  }
  if (traits.supports_color_capability(ColorCapability::COLOR_TEMPERATURE)) {
    ESP_LOGCONFIG(TAG,
                  "  Min Mireds: %.1f\n"
                  "  Max Mireds: %.1f",
                  traits.get_min_mireds(), traits.get_max_mireds());
  }
}
void LightState::loop() {
  // Apply effect (if any)
  auto *effect = this->get_active_effect_();
  if (effect != nullptr) {
    effect->apply();
  }

  // Apply transformer (if any)
  if (this->transformer_ != nullptr) {
    auto values = this->transformer_->apply();
    this->is_transformer_active_ = true;
    if (values.has_value()) {
      this->current_values = *values;
      this->output_->update_state(this);
      this->next_write_ = true;
    }

    const bool finished = this->transformer_->is_finished();
#ifdef USE_LIGHT_TRANSITION_PUBLISH_INTERVAL
    if (this->transition_publish_enabled_ && !finished) {
      const uint32_t now = App.get_loop_component_start_time();
      if (now - this->last_transition_state_publish_ >= this->transition_state_publish_interval_) {
        this->publish_state();
        this->last_transition_state_publish_ = now;
      }
    }
#endif

    if (finished) {
      // if the transition has written directly to the output, current_values is outdated, so update it
      this->current_values = this->transformer_->get_target_values();
      this->transformer_->stop();
      this->is_transformer_active_ = false;
      this->transformer_ = nullptr;
#ifdef USE_LIGHT_TRANSITION_PUBLISH_INTERVAL
      if (this->transition_publish_enabled_) {
        // Report the end state from remote_values; a flash's stop() left publishing to us
        this->transition_publish_enabled_ = false;
        this->publish_state();
      }
#endif
      if (this->target_state_reached_listeners_) {
        for (auto *listener : *this->target_state_reached_listeners_) {
          listener->on_light_target_state_reached();
        }
      }

      // Disable loop if idle (no transformer and no effect)
      this->disable_loop_if_idle_();
    }
  }

  // Write state to the light
  if (this->next_write_) {
    this->next_write_ = false;
    this->output_->write_state(this);
    // Disable loop if idle (no transformer and no effect)
    this->disable_loop_if_idle_();
  }
}

void LightState::publish_state() {
  if (this->remote_values_listeners_) {
    for (auto *listener : *this->remote_values_listeners_) {
      listener->on_light_remote_values_update();
    }
  }
#if defined(USE_LIGHT) && defined(USE_CONTROLLER_REGISTRY)
  ControllerRegistry::notify_light_update(this);
#endif
}

LightOutput *LightState::get_output() const { return this->output_; }

static constexpr auto EFFECT_NONE_REF = StringRef::from_lit("None");

StringRef LightState::get_effect_name() {
  if (this->active_effect_index_ > 0) {
    return this->effects_[this->active_effect_index_ - 1]->get_name();
  }
  return EFFECT_NONE_REF;
}

void LightState::add_remote_values_listener(LightRemoteValuesListener *listener) {
  if (!this->remote_values_listeners_) {
    this->remote_values_listeners_ = make_unique<std::vector<LightRemoteValuesListener *>>();
  }
  this->remote_values_listeners_->push_back(listener);
}
void LightState::add_target_state_reached_listener(LightTargetStateReachedListener *listener) {
  if (!this->target_state_reached_listeners_) {
    this->target_state_reached_listeners_ = make_unique<std::vector<LightTargetStateReachedListener *>>();
  }
  this->target_state_reached_listeners_->push_back(listener);
}

void LightState::add_effects(const std::initializer_list<LightEffect *> &effects) {
  // Called once from Python codegen during setup with all effects from YAML config
  this->effects_ = effects;
}

void LightState::current_values_as_brightness(float *brightness) {
  this->current_values.as_brightness(brightness);
  *brightness = this->gamma_correct_lut(*brightness);
}
void LightState::current_values_as_rgb(float *red, float *green, float *blue) {
  this->current_values.as_rgb(red, green, blue);
  *red = this->gamma_correct_lut(*red);
  *green = this->gamma_correct_lut(*green);
  *blue = this->gamma_correct_lut(*blue);
}
void LightState::current_values_as_rgbw(float *red, float *green, float *blue, float *white) {
  this->current_values.as_rgbw(red, green, blue, white);
  *red = this->gamma_correct_lut(*red);
  *green = this->gamma_correct_lut(*green);
  *blue = this->gamma_correct_lut(*blue);
  *white = this->gamma_correct_lut(*white);
}
void LightState::current_values_as_rgbww(float *red, float *green, float *blue, float *cold_white, float *warm_white,
                                         bool constant_brightness) {
  this->current_values.as_rgb(red, green, blue);
  *red = this->gamma_correct_lut(*red);
  *green = this->gamma_correct_lut(*green);
  *blue = this->gamma_correct_lut(*blue);
  this->current_values_as_cwww(cold_white, warm_white, constant_brightness);
}
void LightState::current_values_as_rgbct(float *red, float *green, float *blue, float *color_temperature,
                                         float *white_brightness) {
  auto traits = this->get_traits();
  this->current_values.as_rgbct(traits.get_min_mireds(), traits.get_max_mireds(), red, green, blue, color_temperature,
                                white_brightness);
  *red = this->gamma_correct_lut(*red);
  *green = this->gamma_correct_lut(*green);
  *blue = this->gamma_correct_lut(*blue);
  *white_brightness = this->gamma_correct_lut(*white_brightness);
}
void LightState::current_values_as_cwww(float *cold_white, float *warm_white, bool constant_brightness) {
  if (!constant_brightness) {
    // Without constant_brightness, gamma commutes with simple multiplication:
    //   gamma(white_level * cw) = gamma(white_level) * gamma(cw)
    // (since gamma(a*b) = (a*b)^g = a^g * b^g = gamma(a) * gamma(b))
    // so applying gamma after is mathematically equivalent and simpler.
    this->current_values.as_cwww(cold_white, warm_white, false);
    *cold_white = this->gamma_correct_lut(*cold_white);
    *warm_white = this->gamma_correct_lut(*warm_white);
    return;
  }

  // For constant_brightness mode, gamma MUST be applied to the individual
  // channel values BEFORE the balancing formula (max/sum ratio), not after.
  //
  // Why: The cold_white_ and warm_white_ values stored in LightColorValues
  // are gamma-uncorrected (see transform_parameters_() which applies
  // gamma_uncorrect to the linear CW/WW fractions derived from color
  // temperature). Applying gamma_correct here recovers the original linear
  // fractions, which the constant_brightness formula then uses to distribute
  // power evenly. The max/sum formula ensures cold+warm PWM output sums to
  // a constant, keeping total power (and perceived brightness) the same
  // across all color temperatures.
  //
  // Applying gamma AFTER the formula would be incorrect because gamma is
  // nonlinear: gamma(a/b) != gamma(a)/gamma(b), so the carefully balanced
  // ratio would be distorted, causing a severe brightness dip at mid-range
  // color temperatures.
  const auto &v = this->current_values;
  if (!(v.get_color_mode() & ColorCapability::COLD_WARM_WHITE)) {
    *cold_white = *warm_white = 0;
    return;
  }

  const float cw_level = this->gamma_correct_lut(v.get_cold_white());
  const float ww_level = this->gamma_correct_lut(v.get_warm_white());
  const float white_level = this->gamma_correct_lut(v.get_state() * v.get_brightness());
  const float sum = cw_level > 0 || ww_level > 0 ? cw_level + ww_level : 1;  // Don't divide by zero.
  *cold_white = white_level * std::max(cw_level, ww_level) * cw_level / sum;
  *warm_white = white_level * std::max(cw_level, ww_level) * ww_level / sum;
}
void LightState::current_values_as_ct(float *color_temperature, float *white_brightness) {
  auto traits = this->get_traits();
  this->current_values.as_ct(traits.get_min_mireds(), traits.get_max_mireds(), color_temperature, white_brightness);
  *white_brightness = this->gamma_correct_lut(*white_brightness);
}

float LightState::get_gamma_correct() const {
#ifdef USE_LIGHT_GAMMA_LUT
  if (this->gamma_table_ != nullptr)
    return progmem_read_uint16(&this->gamma_table_->gamma_x100) * 0.01f;
#endif  // USE_LIGHT_GAMMA_LUT
  return 0.0f;
}

#ifdef USE_LIGHT_GAMMA_LUT
float LightState::gamma_correct_lut(float value) const {
  if (value <= 0.0f)
    return 0.0f;
  if (value >= 1.0f)
    return 1.0f;
  if (this->gamma_table_ == nullptr)
    return value;
  float scaled = value * 255.0f;
  auto idx = static_cast<uint8_t>(scaled);
  if (idx >= 255)
    return progmem_read_uint16(&this->gamma_table_->lut[255]) / 65535.0f;
  float frac = scaled - idx;
  float a = progmem_read_uint16(&this->gamma_table_->lut[idx]);
  float b = progmem_read_uint16(&this->gamma_table_->lut[idx + 1]);
  return (a + frac * (b - a)) / 65535.0f;
}
float LightState::gamma_uncorrect_lut(float value) const {
  if (value <= 0.0f)
    return 0.0f;
  if (value >= 1.0f)
    return 1.0f;
  if (this->gamma_table_ == nullptr)
    return value;
  uint16_t target = static_cast<uint16_t>(value * 65535.0f);
  uint8_t lo = gamma_table_reverse_search(this->gamma_table_->lut, target);
  if (lo >= 255)
    return 1.0f;
  // Interpolate between lo and lo+1
  uint16_t a = progmem_read_uint16(&this->gamma_table_->lut[lo]);
  uint16_t b = progmem_read_uint16(&this->gamma_table_->lut[lo + 1]);
  if (b == a)
    return lo / 255.0f;
  float frac = static_cast<float>(target - a) / static_cast<float>(b - a);
  return (lo + frac) / 255.0f;
}
#endif  // USE_LIGHT_GAMMA_LUT

void LightState::start_effect_(uint32_t effect_index) {
  // An external add_effects() can exceed the codegen cap; ignore an index the uint16_t can't hold
  if (effect_index > std::numeric_limits<uint16_t>::max())
    return;
  this->stop_effect_();
  if (effect_index == 0)
    return;

  this->active_effect_index_ = static_cast<uint16_t>(effect_index);
  auto *effect = this->get_active_effect_();
  effect->start_internal();
  // Enable loop while effect is active
  this->enable_loop();
}
LightEffect *LightState::get_active_effect_() {
  if (this->active_effect_index_ == 0) {
    return nullptr;
  } else {
    return this->effects_[this->active_effect_index_ - 1];
  }
}
void LightState::stop_effect_() {
  auto *effect = this->get_active_effect_();
  if (effect != nullptr) {
    effect->stop();
  }
  this->active_effect_index_ = 0;
  // Disable loop if idle (no effect and no transformer)
  this->disable_loop_if_idle_();
}

void LightState::start_transition_(const LightColorValues &target, uint32_t length, bool set_remote_values) {
  this->transformer_ = this->output_->create_default_transition();
  this->transformer_->setup(this->current_values, target, length);
  this->set_transformer_remote_values_(target, set_remote_values);
  // Enable loop while transition is active
  this->enable_loop();
}

void LightState::start_flash_(const LightColorValues &target, uint32_t length, bool set_remote_values) {
  LightColorValues end_colors = this->remote_values;
  // If starting a flash if one is already happening, set end values to end values of current flash
  // Hacky but works
  if (this->transformer_ != nullptr)
    end_colors = this->transformer_->get_start_values();

  this->transformer_ = make_unique<LightFlashTransformer>(*this);
  this->transformer_->setup(end_colors, target, length);
  this->set_transformer_remote_values_(target, set_remote_values);
  // Enable loop while flash is active
  this->enable_loop();
}

void LightState::set_immediately_(const LightColorValues &target, bool set_remote_values) {
  this->is_transformer_active_ = false;
  this->transformer_ = nullptr;
#ifdef USE_LIGHT_TRANSITION_PUBLISH_INTERVAL
  this->transition_publish_enabled_ = false;
#endif
  this->current_values = target;
  if (set_remote_values) {
    this->remote_values = target;
  }
  this->output_->update_state(this);
  this->schedule_write_();
}

void LightState::disable_loop_if_idle_() {
  // Only disable loop if both transformer and effect are inactive, and no pending writes
  if (this->transformer_ == nullptr && this->get_active_effect_() == nullptr && !this->next_write_) {
    this->disable_loop();
  }
}

#ifdef USE_LIGHT_TRANSITION_PUBLISH_INTERVAL
void LightState::set_transformer_remote_values_(const LightColorValues &target, bool set_remote_values) {
  this->transition_publish_enabled_ = set_remote_values && this->transition_state_publish_interval_ > 0;
  if (this->transition_publish_enabled_) {
    this->last_transition_state_publish_ = App.get_loop_component_start_time();
  }
  if (set_remote_values) {
    this->remote_values = target;
  }
}
#endif

void LightState::save_remote_values_() {
  if (!this->save_enabled_)
    return;
  LightStateRTCState saved;
  saved.color_mode = this->remote_values.get_color_mode();
  // Always the real on/off status (RESTORE_AND_ON/OFF used to persist a hardcoded
  // true/false here instead; harmless, since those modes force `state` again on
  // every load regardless of what was saved -- see _legacy_restore_statements).
  saved.state = this->remote_values.is_on();
  saved.brightness = this->remote_values.get_brightness();
  saved.color_brightness = this->remote_values.get_color_brightness();
  saved.red = this->remote_values.get_red();
  saved.green = this->remote_values.get_green();
  saved.blue = this->remote_values.get_blue();
  saved.white = this->remote_values.get_white();
  saved.color_temp = this->remote_values.get_color_temperature();
  saved.cold_white = this->remote_values.get_cold_white();
  saved.warm_white = this->remote_values.get_warm_white();
  saved.effect = static_cast<uint32_t>(this->active_effect_index_);  // the saved layout stays uint32_t
  this->rtc_.save(&saved);
}

}  // namespace esphome::light
