#include <cinttypes>
#include "light_call.h"
#include "light_state.h"
#include "esphome/core/log.h"

namespace esphome {
namespace light {

static const char *const TAG = "light";

static const LogString *color_mode_to_human(ColorMode color_mode) {
  if (color_mode == ColorMode::UNKNOWN)
    return LOG_STR("Unknown");
  if (color_mode == ColorMode::WHITE)
    return LOG_STR("White");
  if (color_mode == ColorMode::COLOR_TEMPERATURE)
    return LOG_STR("Color temperature");
  if (color_mode == ColorMode::COLD_WARM_WHITE)
    return LOG_STR("Cold/warm white");
  if (color_mode == ColorMode::RGB)
    return LOG_STR("RGB");
  if (color_mode == ColorMode::RGB_WHITE)
    return LOG_STR("RGBW");
  if (color_mode == ColorMode::RGB_COLD_WARM_WHITE)
    return LOG_STR("RGB + cold/warm white");
  if (color_mode == ColorMode::RGB_COLOR_TEMPERATURE)
    return LOG_STR("RGB + color temperature");
  return LOG_STR("");
}

void LightCall::perform() {
  const char *name = this->parent_->get_name().c_str();
  LightColorValues v = this->validate_();

  if (this->publish_) {
    ESP_LOGD(TAG, "'%s' Setting:", name);

    // Only print color mode when it's being changed
    ColorMode current_color_mode = this->parent_->remote_values.get_color_mode();
    if (this->color_mode_.value_or(current_color_mode) != current_color_mode) {
      ESP_LOGD(TAG, "  Color mode: %s", LOG_STR_ARG(color_mode_to_human(v.get_color_mode())));
    }

    // Only print state when it's being changed
    bool current_state = this->parent_->remote_values.is_on();
    if (this->state_.value_or(current_state) != current_state) {
      ESP_LOGD(TAG, "  State: %s", ONOFF(v.is_on()));
    }

    if (this->brightness_.has_value()) {
      ESP_LOGD(TAG, "  Brightness: %.0f%%", v.get_brightness() * 100.0f);
    }

    if (this->color_brightness_.has_value()) {
      ESP_LOGD(TAG, "  Color brightness: %.0f%%", v.get_color_brightness() * 100.0f);
    }
    if (this->red_.has_value() || this->green_.has_value() || this->blue_.has_value()) {
      ESP_LOGD(TAG, "  Red: %.0f%%, Green: %.0f%%, Blue: %.0f%%", v.get_red() * 100.0f, v.get_green() * 100.0f,
               v.get_blue() * 100.0f);
    }

    if (this->white_.has_value()) {
      ESP_LOGD(TAG, "  White: %.0f%%", v.get_white() * 100.0f);
    }
    if (this->color_temperature_.has_value()) {
      ESP_LOGD(TAG, "  Color temperature: %.1f mireds", v.get_color_temperature());
    }

    if (this->cold_white_.has_value() || this->warm_white_.has_value()) {
      ESP_LOGD(TAG, "  Cold white: %.0f%%, warm white: %.0f%%", v.get_cold_white() * 100.0f,
               v.get_warm_white() * 100.0f);
    }
  }

  if (this->has_flash_()) {
    // FLASH
    if (this->publish_) {
      ESP_LOGD(TAG, "  Flash length: %.1fs", *this->flash_length_ / 1e3f);
    }

    this->parent_->start_flash_(v, *this->flash_length_, this->publish_);
  } else if (this->has_transition_()) {
    // TRANSITION
    if (this->publish_) {
      ESP_LOGD(TAG, "  Transition length: %.1fs", *this->transition_length_ / 1e3f);
    }

    // Special case: Transition and effect can be set when turning off
    if (this->has_effect_()) {
      if (this->publish_) {
        ESP_LOGD(TAG, "  Effect: 'None'");
      }
      this->parent_->stop_effect_();
    }

    this->parent_->start_transition_(v, *this->transition_length_, this->publish_);

  } else if (this->has_effect_()) {
    // EFFECT
    auto effect = this->effect_;
    const char *effect_s;
    if (effect == 0u) {
      effect_s = "None";
    } else {
      effect_s = this->parent_->effects_[*this->effect_ - 1]->get_name().c_str();
    }

    if (this->publish_) {
      ESP_LOGD(TAG, "  Effect: '%s'", effect_s);
    }

    this->parent_->start_effect_(*this->effect_);

    // Also set light color values when starting an effect
    // For example to turn off the light
    this->parent_->set_immediately_(v, true);
  } else {
    // INSTANT CHANGE
    this->parent_->set_immediately_(v, this->publish_);
  }

  if (!this->has_transition_()) {
    this->parent_->target_state_reached_callback_.call();
  }
  if (this->publish_) {
    this->parent_->publish_state();
  }
  if (this->save_) {
    this->parent_->save_remote_values_();
  }
}

LightColorValues LightCall::validate_() {
  auto *name = this->parent_->get_name().c_str();
  auto traits = this->parent_->get_traits();

  // Color mode check
  if (this->color_mode_.has_value() && !traits.supports_color_mode(this->color_mode_.value())) {
    ESP_LOGW(TAG, "'%s' - This light does not support color mode %s!", name,
             LOG_STR_ARG(color_mode_to_human(this->color_mode_.value())));
    this->color_mode_.reset();
  }

  // Ensure there is always a color mode set
  if (!this->color_mode_.has_value()) {
    this->color_mode_ = this->compute_color_mode_();
  }
  auto color_mode = *this->color_mode_;

  // Transform calls that use non-native parameters for the current mode.
  this->transform_parameters_();

  // Brightness exists check
  if (this->brightness_.has_value() && *this->brightness_ > 0.0f && !(color_mode & ColorCapability::BRIGHTNESS)) {
    ESP_LOGW(TAG, "'%s' - This light does not support setting brightness!", name);
    this->brightness_.reset();
  }

  // Transition length possible check
  if (this->transition_length_.has_value() && *this->transition_length_ != 0 &&
      !(color_mode & ColorCapability::BRIGHTNESS)) {
    ESP_LOGW(TAG, "'%s' - This light does not support transitions!", name);
    this->transition_length_.reset();
  }

  // Color brightness exists check
  if (this->color_brightness_.has_value() && *this->color_brightness_ > 0.0f && !(color_mode & ColorCapability::RGB)) {
    ESP_LOGW(TAG, "'%s' - This color mode does not support setting RGB brightness!", name);
    this->color_brightness_.reset();
  }

  // RGB exists check
  if ((this->red_.has_value() && *this->red_ > 0.0f) || (this->green_.has_value() && *this->green_ > 0.0f) ||
      (this->blue_.has_value() && *this->blue_ > 0.0f)) {
    if (!(color_mode & ColorCapability::RGB)) {
      ESP_LOGW(TAG, "'%s' - This color mode does not support setting RGB color!", name);
      this->red_.reset();
      this->green_.reset();
      this->blue_.reset();
    }
  }

  // White value exists check
  if (this->white_.has_value() && *this->white_ > 0.0f &&
      !(color_mode & ColorCapability::WHITE || color_mode & ColorCapability::COLD_WARM_WHITE)) {
    ESP_LOGW(TAG, "'%s' - This color mode does not support setting white value!", name);
    this->white_.reset();
  }

  // Color temperature exists check
  if (this->color_temperature_.has_value() &&
      !(color_mode & ColorCapability::COLOR_TEMPERATURE || color_mode & ColorCapability::COLD_WARM_WHITE)) {
    ESP_LOGW(TAG, "'%s' - This color mode does not support setting color temperature!", name);
    this->color_temperature_.reset();
  }

  // Cold/warm white value exists check
  if ((this->cold_white_.has_value() && *this->cold_white_ > 0.0f) ||
      (this->warm_white_.has_value() && *this->warm_white_ > 0.0f)) {
    if (!(color_mode & ColorCapability::COLD_WARM_WHITE)) {
      ESP_LOGW(TAG, "'%s' - This color mode does not support setting cold/warm white value!", name);
      this->cold_white_.reset();
      this->warm_white_.reset();
    }
  }

#define VALIDATE_RANGE_(name_, upper_name, min, max) \
  if (name_##_.has_value()) { \
    auto val = *name_##_; \
    if (val < (min) || val > (max)) { \
      ESP_LOGW(TAG, "'%s' - %s value %.2f is out of range [%.1f - %.1f]!", name, LOG_STR_LITERAL(upper_name), val, \
               (min), (max)); \
      name_##_ = clamp(val, (min), (max)); \
    } \
  }
#define VALIDATE_RANGE(name, upper_name) VALIDATE_RANGE_(name, upper_name, 0.0f, 1.0f)

  // Range checks
  VALIDATE_RANGE(brightness, "Brightness")
  VALIDATE_RANGE(color_brightness, "Color brightness")
  VALIDATE_RANGE(red, "Red")
  VALIDATE_RANGE(green, "Green")
  VALIDATE_RANGE(blue, "Blue")
  VALIDATE_RANGE(white, "White")
  VALIDATE_RANGE(cold_white, "Cold white")
  VALIDATE_RANGE(warm_white, "Warm white")
  VALIDATE_RANGE_(color_temperature, "Color temperature", traits.get_min_mireds(), traits.get_max_mireds())

  // Flag whether an explicit turn off was requested, in which case we'll also stop the effect.
  bool explicit_turn_off_request = this->state_.has_value() && !*this->state_;

  // Turn off when brightness is set to zero, and reset brightness (so that it has nonzero brightness when turned on).
  if (this->brightness_.has_value() && *this->brightness_ == 0.0f) {
    this->state_ = optional<float>(false);
    this->brightness_ = optional<float>(1.0f);
  }

  // Set color brightness to 100% if currently zero and a color is set.
  if (this->red_.has_value() || this->green_.has_value() || this->blue_.has_value()) {
    if (!this->color_brightness_.has_value() && this->parent_->remote_values.get_color_brightness() == 0.0f)
      this->color_brightness_ = optional<float>(1.0f);
  }

  // Create color values for the light with this call applied.
  auto v = this->parent_->remote_values;
  if (this->color_mode_.has_value())
    v.set_color_mode(*this->color_mode_);
  if (this->state_.has_value())
    v.set_state(*this->state_);
  if (this->brightness_.has_value())
    v.set_brightness(*this->brightness_);
  if (this->color_brightness_.has_value())
    v.set_color_brightness(*this->color_brightness_);
  if (this->red_.has_value())
    v.set_red(*this->red_);
  if (this->green_.has_value())
    v.set_green(*this->green_);
  if (this->blue_.has_value())
    v.set_blue(*this->blue_);
  if (this->white_.has_value())
    v.set_white(*this->white_);
  if (this->color_temperature_.has_value())
    v.set_color_temperature(*this->color_temperature_);
  if (this->cold_white_.has_value())
    v.set_cold_white(*this->cold_white_);
  if (this->warm_white_.has_value())
    v.set_warm_white(*this->warm_white_);

  v.normalize_color();

  // Flash length check
  if (this->has_flash_() && *this->flash_length_ == 0) {
    ESP_LOGW(TAG, "'%s' - Flash length must be greater than zero!", name);
    this->flash_length_.reset();
  }

  // validate transition length/flash length/effect not used at the same time
  bool supports_transition = color_mode & ColorCapability::BRIGHTNESS;

  // If effect is already active, remove effect start
  if (this->has_effect_() && *this->effect_ == this->parent_->active_effect_index_) {
    this->effect_.reset();
  }

  // validate effect index
  if (this->has_effect_() && *this->effect_ > this->parent_->effects_.size()) {
    ESP_LOGW(TAG, "'%s' - Invalid effect index %" PRIu32 "!", name, *this->effect_);
    this->effect_.reset();
  }

  if (this->has_effect_() && (this->has_transition_() || this->has_flash_())) {
    ESP_LOGW(TAG, "'%s' - Effect cannot be used together with transition/flash!", name);
    this->transition_length_.reset();
    this->flash_length_.reset();
  }

  if (this->has_flash_() && this->has_transition_()) {
    ESP_LOGW(TAG, "'%s' - Flash cannot be used together with transition!", name);
    this->transition_length_.reset();
  }

  if (!this->has_transition_() && !this->has_flash_() && (!this->has_effect_() || *this->effect_ == 0) &&
      supports_transition) {
    // nothing specified and light supports transitions, set default transition length
    this->transition_length_ = this->parent_->default_transition_length_;
  }

  if (this->transition_length_.value_or(0) == 0) {
    // 0 transition is interpreted as no transition (instant change)
    this->transition_length_.reset();
  }

  if (this->has_transition_() && !supports_transition) {
    ESP_LOGW(TAG, "'%s' - Light does not support transitions!", name);
    this->transition_length_.reset();
  }

  // If not a flash and turning the light off, then disable the light
  // Do not use light color values directly, so that effects can set 0% brightness
  // Reason: When user turns off the light in frontend, the effect should also stop
  if (!this->has_flash_() && !this->state_.value_or(v.is_on())) {
    if (this->has_effect_()) {
      ESP_LOGW(TAG, "'%s' - Cannot start an effect when turning off!", name);
      this->effect_.reset();
    } else if (this->parent_->active_effect_index_ != 0 && explicit_turn_off_request) {
      // Auto turn off effect
      this->effect_ = 0;
    }
  }

  // Disable saving for flashes
  if (this->has_flash_())
    this->save_ = false;

  return v;
}
void LightCall::transform_parameters_() {
  auto traits = this->parent_->get_traits();

  // Allow CWWW modes to be set with a white value and/or color temperature. This is used by HA,
  // which doesn't support CWWW modes (yet?), and for compatibility with the pre-colormode model,
  // as CWWW and RGBWW lights used to represent their values as white + color temperature.
  if (((this->white_.has_value() && *this->white_ > 0.0f) || this->color_temperature_.has_value()) &&  //
      (*this->color_mode_ & ColorCapability::COLD_WARM_WHITE) &&                                       //
      !(*this->color_mode_ & ColorCapability::WHITE) &&                                                //
      !(*this->color_mode_ & ColorCapability::COLOR_TEMPERATURE) &&                                    //
      traits.get_min_mireds() > 0.0f && traits.get_max_mireds() > 0.0f) {
    ESP_LOGD(TAG, "'%s' - Setting cold/warm white channels using white/color temperature values.",
             this->parent_->get_name().c_str());
    auto current_values = this->parent_->remote_values;
    if (this->color_temperature_.has_value()) {
      const float white =
          this->white_.value_or(fmaxf(current_values.get_cold_white(), current_values.get_warm_white()));
      const float color_temp = clamp(*this->color_temperature_, traits.get_min_mireds(), traits.get_max_mireds());
      const float ww_fraction =
          (color_temp - traits.get_min_mireds()) / (traits.get_max_mireds() - traits.get_min_mireds());
      const float cw_fraction = 1.0f - ww_fraction;
      const float max_cw_ww = std::max(ww_fraction, cw_fraction);
      this->cold_white_ = white * gamma_uncorrect(cw_fraction / max_cw_ww, this->parent_->get_gamma_correct());
      this->warm_white_ = white * gamma_uncorrect(ww_fraction / max_cw_ww, this->parent_->get_gamma_correct());
    } else {
      const float max_cw_ww = std::max(current_values.get_warm_white(), current_values.get_cold_white());
      this->cold_white_ = *this->white_ * current_values.get_cold_white() / max_cw_ww;
      this->warm_white_ = *this->white_ * current_values.get_warm_white() / max_cw_ww;
    }
  }
}
ColorMode LightCall::compute_color_mode_() {
  auto supported_modes = this->parent_->get_traits().get_supported_color_modes();
  int supported_count = supported_modes.size();

  // Some lights don't support any color modes (e.g. monochromatic light), leave it at unknown.
  if (supported_count == 0)
    return ColorMode::UNKNOWN;

  // In the common case of lights supporting only a single mode, use that one.
  if (supported_count == 1)
    return *supported_modes.begin();

  // Don't change if the light is being turned off.
  ColorMode current_mode = this->parent_->remote_values.get_color_mode();
  if (this->state_.has_value() && !*this->state_)
    return current_mode;

  // If no color mode is specified, we try to guess the color mode. This is needed for backward compatibility to
  // pre-colormode clients and automations, but also for the MQTT API, where HA doesn't let us know which color mode
  // was used for some reason.
  std::set<ColorMode> suitable_modes = this->get_suitable_color_modes_();

  // Don't change if the current mode is suitable.
  if (suitable_modes.count(current_mode) > 0) {
    ESP_LOGI(TAG, "'%s' - Keeping current color mode %s for call without color mode.",
             this->parent_->get_name().c_str(), LOG_STR_ARG(color_mode_to_human(current_mode)));
    return current_mode;
  }

  // Use the preferred suitable mode.
  for (auto mode : suitable_modes) {
    if (supported_modes.count(mode) == 0)
      continue;

    ESP_LOGI(TAG, "'%s' - Using color mode %s for call without color mode.", this->parent_->get_name().c_str(),
             LOG_STR_ARG(color_mode_to_human(mode)));
    return mode;
  }

  // There's no supported mode for this call, so warn, use the current more or a mode at random and let validation strip
  // out whatever we don't support.
  auto color_mode = current_mode != ColorMode::UNKNOWN ? current_mode : *supported_modes.begin();
  ESP_LOGW(TAG, "'%s' - No color mode suitable for this call supported, defaulting to %s!",
           this->parent_->get_name().c_str(), LOG_STR_ARG(color_mode_to_human(color_mode)));
  return color_mode;
}
std::set<ColorMode> LightCall::get_suitable_color_modes_() {
  bool has_white = this->white_.has_value() && *this->white_ > 0.0f;
  bool has_ct = this->color_temperature_.has_value();
  bool has_cwww = (this->cold_white_.has_value() && *this->cold_white_ > 0.0f) ||
                  (this->warm_white_.has_value() && *this->warm_white_ > 0.0f);
  bool has_rgb = (this->color_brightness_.has_value() && *this->color_brightness_ > 0.0f) ||
                 (this->red_.has_value() || this->green_.has_value() || this->blue_.has_value());

#define KEY(white, ct, cwww, rgb) ((white) << 0 | (ct) << 1 | (cwww) << 2 | (rgb) << 3)
#define ENTRY(white, ct, cwww, rgb, ...) \
  std::make_tuple<uint8_t, std::set<ColorMode>>(KEY(white, ct, cwww, rgb), __VA_ARGS__)

  // Flag order: white, color temperature, cwww, rgb
  std::array<std::tuple<uint8_t, std::set<ColorMode>>, 10> lookup_table{
      ENTRY(true, false, false, false,
            {ColorMode::WHITE, ColorMode::RGB_WHITE, ColorMode::RGB_COLOR_TEMPERATURE, ColorMode::COLD_WARM_WHITE,
             ColorMode::RGB_COLD_WARM_WHITE}),
      ENTRY(false, true, false, false,
            {ColorMode::COLOR_TEMPERATURE, ColorMode::RGB_COLOR_TEMPERATURE, ColorMode::COLD_WARM_WHITE,
             ColorMode::RGB_COLD_WARM_WHITE}),
      ENTRY(true, true, false, false,
            {ColorMode::COLD_WARM_WHITE, ColorMode::RGB_COLOR_TEMPERATURE, ColorMode::RGB_COLD_WARM_WHITE}),
      ENTRY(false, false, true, false, {ColorMode::COLD_WARM_WHITE, ColorMode::RGB_COLD_WARM_WHITE}),
      ENTRY(false, false, false, false,
            {ColorMode::RGB_WHITE, ColorMode::RGB_COLOR_TEMPERATURE, ColorMode::RGB_COLD_WARM_WHITE, ColorMode::RGB,
             ColorMode::WHITE, ColorMode::COLOR_TEMPERATURE, ColorMode::COLD_WARM_WHITE}),
      ENTRY(true, false, false, true,
            {ColorMode::RGB_WHITE, ColorMode::RGB_COLOR_TEMPERATURE, ColorMode::RGB_COLD_WARM_WHITE}),
      ENTRY(false, true, false, true, {ColorMode::RGB_COLOR_TEMPERATURE, ColorMode::RGB_COLD_WARM_WHITE}),
      ENTRY(true, true, false, true, {ColorMode::RGB_COLOR_TEMPERATURE, ColorMode::RGB_COLD_WARM_WHITE}),
      ENTRY(false, false, true, true, {ColorMode::RGB_COLD_WARM_WHITE}),
      ENTRY(false, false, false, true,
            {ColorMode::RGB, ColorMode::RGB_WHITE, ColorMode::RGB_COLOR_TEMPERATURE, ColorMode::RGB_COLD_WARM_WHITE}),
  };

  auto key = KEY(has_white, has_ct, has_cwww, has_rgb);
  for (auto &item : lookup_table) {
    if (std::get<0>(item) == key)
      return std::get<1>(item);
  }

  // This happens if there are conflicting flags given.
  return {};
}

LightCall &LightCall::set_effect(const std::string &effect) {
  if (strcasecmp(effect.c_str(), "none") == 0) {
    this->set_effect(0);
    return *this;
  }

  bool found = false;
  for (uint32_t i = 0; i < this->parent_->effects_.size(); i++) {
    LightEffect *e = this->parent_->effects_[i];

    if (strcasecmp(effect.c_str(), e->get_name().c_str()) == 0) {
      this->set_effect(i + 1);
      found = true;
      break;
    }
  }
  if (!found) {
    ESP_LOGW(TAG, "'%s' - No such effect '%s'", this->parent_->get_name().c_str(), effect.c_str());
  }
  return *this;
}
LightCall &LightCall::from_light_color_values(const LightColorValues &values) {
  this->set_state(values.is_on());
  this->set_brightness_if_supported(values.get_brightness());
  this->set_color_brightness_if_supported(values.get_color_brightness());
  this->set_color_mode_if_supported(values.get_color_mode());
  this->set_red_if_supported(values.get_red());
  this->set_green_if_supported(values.get_green());
  this->set_blue_if_supported(values.get_blue());
  this->set_white_if_supported(values.get_white());
  this->set_color_temperature_if_supported(values.get_color_temperature());
  this->set_cold_white_if_supported(values.get_cold_white());
  this->set_warm_white_if_supported(values.get_warm_white());
  return *this;
}
ColorMode LightCall::get_active_color_mode_() {
  return this->color_mode_.value_or(this->parent_->remote_values.get_color_mode());
}
LightCall &LightCall::set_transition_length_if_supported(uint32_t transition_length) {
  if (this->get_active_color_mode_() & ColorCapability::BRIGHTNESS)
    this->set_transition_length(transition_length);
  return *this;
}
LightCall &LightCall::set_brightness_if_supported(float brightness) {
  if (this->get_active_color_mode_() & ColorCapability::BRIGHTNESS)
    this->set_brightness(brightness);
  return *this;
}
LightCall &LightCall::set_color_mode_if_supported(ColorMode color_mode) {
  if (this->parent_->get_traits().supports_color_mode(color_mode))
    this->color_mode_ = color_mode;
  return *this;
}
LightCall &LightCall::set_color_brightness_if_supported(float brightness) {
  if (this->get_active_color_mode_() & ColorCapability::RGB)
    this->set_color_brightness(brightness);
  return *this;
}
LightCall &LightCall::set_red_if_supported(float red) {
  if (this->get_active_color_mode_() & ColorCapability::RGB)
    this->set_red(red);
  return *this;
}
LightCall &LightCall::set_green_if_supported(float green) {
  if (this->get_active_color_mode_() & ColorCapability::RGB)
    this->set_green(green);
  return *this;
}
LightCall &LightCall::set_blue_if_supported(float blue) {
  if (this->get_active_color_mode_() & ColorCapability::RGB)
    this->set_blue(blue);
  return *this;
}
LightCall &LightCall::set_white_if_supported(float white) {
  if (this->get_active_color_mode_() & ColorCapability::WHITE)
    this->set_white(white);
  return *this;
}
LightCall &LightCall::set_color_temperature_if_supported(float color_temperature) {
  if (this->get_active_color_mode_() & ColorCapability::COLOR_TEMPERATURE ||
      this->get_active_color_mode_() & ColorCapability::COLD_WARM_WHITE)
    this->set_color_temperature(color_temperature);
  return *this;
}
LightCall &LightCall::set_cold_white_if_supported(float cold_white) {
  if (this->get_active_color_mode_() & ColorCapability::COLD_WARM_WHITE)
    this->set_cold_white(cold_white);
  return *this;
}
LightCall &LightCall::set_warm_white_if_supported(float warm_white) {
  if (this->get_active_color_mode_() & ColorCapability::COLD_WARM_WHITE)
    this->set_warm_white(warm_white);
  return *this;
}
LightCall &LightCall::set_state(optional<bool> state) {
  this->state_ = state;
  return *this;
}
LightCall &LightCall::set_state(bool state) {
  this->state_ = state;
  return *this;
}
LightCall &LightCall::set_transition_length(optional<uint32_t> transition_length) {
  this->transition_length_ = transition_length;
  return *this;
}
LightCall &LightCall::set_transition_length(uint32_t transition_length) {
  this->transition_length_ = transition_length;
  return *this;
}
LightCall &LightCall::set_flash_length(optional<uint32_t> flash_length) {
  this->flash_length_ = flash_length;
  return *this;
}
LightCall &LightCall::set_flash_length(uint32_t flash_length) {
  this->flash_length_ = flash_length;
  return *this;
}
LightCall &LightCall::set_brightness(optional<float> brightness) {
  this->brightness_ = brightness;
  return *this;
}
LightCall &LightCall::set_brightness(float brightness) {
  this->brightness_ = brightness;
  return *this;
}
LightCall &LightCall::set_color_mode(optional<ColorMode> color_mode) {
  this->color_mode_ = color_mode;
  return *this;
}
LightCall &LightCall::set_color_mode(ColorMode color_mode) {
  this->color_mode_ = color_mode;
  return *this;
}
LightCall &LightCall::set_color_brightness(optional<float> brightness) {
  this->color_brightness_ = brightness;
  return *this;
}
LightCall &LightCall::set_color_brightness(float brightness) {
  this->color_brightness_ = brightness;
  return *this;
}
LightCall &LightCall::set_red(optional<float> red) {
  this->red_ = red;
  return *this;
}
LightCall &LightCall::set_red(float red) {
  this->red_ = red;
  return *this;
}
LightCall &LightCall::set_green(optional<float> green) {
  this->green_ = green;
  return *this;
}
LightCall &LightCall::set_green(float green) {
  this->green_ = green;
  return *this;
}
LightCall &LightCall::set_blue(optional<float> blue) {
  this->blue_ = blue;
  return *this;
}
LightCall &LightCall::set_blue(float blue) {
  this->blue_ = blue;
  return *this;
}
LightCall &LightCall::set_white(optional<float> white) {
  this->white_ = white;
  return *this;
}
LightCall &LightCall::set_white(float white) {
  this->white_ = white;
  return *this;
}
LightCall &LightCall::set_color_temperature(optional<float> color_temperature) {
  this->color_temperature_ = color_temperature;
  return *this;
}
LightCall &LightCall::set_color_temperature(float color_temperature) {
  this->color_temperature_ = color_temperature;
  return *this;
}
LightCall &LightCall::set_cold_white(optional<float> cold_white) {
  this->cold_white_ = cold_white;
  return *this;
}
LightCall &LightCall::set_cold_white(float cold_white) {
  this->cold_white_ = cold_white;
  return *this;
}
LightCall &LightCall::set_warm_white(optional<float> warm_white) {
  this->warm_white_ = warm_white;
  return *this;
}
LightCall &LightCall::set_warm_white(float warm_white) {
  this->warm_white_ = warm_white;
  return *this;
}
LightCall &LightCall::set_effect(optional<std::string> effect) {
  if (effect.has_value())
    this->set_effect(*effect);
  return *this;
}
LightCall &LightCall::set_effect(uint32_t effect_number) {
  this->effect_ = effect_number;
  return *this;
}
LightCall &LightCall::set_effect(optional<uint32_t> effect_number) {
  this->effect_ = effect_number;
  return *this;
}
LightCall &LightCall::set_publish(bool publish) {
  this->publish_ = publish;
  return *this;
}
LightCall &LightCall::set_save(bool save) {
  this->save_ = save;
  return *this;
}
LightCall &LightCall::set_rgb(float red, float green, float blue) {
  this->set_red(red);
  this->set_green(green);
  this->set_blue(blue);
  return *this;
}
LightCall &LightCall::set_rgbw(float red, float green, float blue, float white) {
  this->set_rgb(red, green, blue);
  this->set_white(white);
  return *this;
}

}  // namespace light
}  // namespace esphome
