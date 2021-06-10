#include "light_call.h"
#include "light_state.h"
#include "esphome/core/log.h"

namespace esphome {
namespace light {

static const char *const TAG = "light";

#ifdef USE_JSON
LightCall &LightCall::parse_color_json(JsonObject &root) {
  if (root.containsKey("state")) {
    auto val = parse_on_off(root["state"]);
    switch (val) {
      case PARSE_ON:
        this->set_state(true);
        break;
      case PARSE_OFF:
        this->set_state(false);
        break;
      case PARSE_TOGGLE:
        this->set_state(!this->parent_->remote_values.is_on());
        break;
      case PARSE_NONE:
        break;
    }
  }

  if (root.containsKey("brightness")) {
    this->set_brightness(float(root["brightness"]) / 255.0f);
  }

  if (root.containsKey("color")) {
    JsonObject &color = root["color"];
    if (color.containsKey("r")) {
      this->set_red(float(color["r"]) / 255.0f);
    }
    if (color.containsKey("g")) {
      this->set_green(float(color["g"]) / 255.0f);
    }
    if (color.containsKey("b")) {
      this->set_blue(float(color["b"]) / 255.0f);
    }
  }

  if (root.containsKey("color_brightness")) {
    this->set_color_brightness(float(root["color_brightness"]) / 255.0f);
  }

  if (root.containsKey("white_value")) {
    this->set_white(float(root["white_value"]) / 255.0f);
  }

  if (root.containsKey("color_temp")) {
    this->set_color_temperature(float(root["color_temp"]));
  }

  return *this;
}
LightCall &LightCall::parse_json(JsonObject &root) {
  this->parse_color_json(root);

  if (root.containsKey("flash")) {
    auto length = uint32_t(float(root["flash"]) * 1000);
    this->set_flash_length(length);
  }

  if (root.containsKey("transition")) {
    auto length = uint32_t(float(root["transition"]) * 1000);
    this->set_transition_length(length);
  }

  if (root.containsKey("effect")) {
    const char *effect = root["effect"];
    this->set_effect(effect);
  }

  return *this;
}
#endif

void LightCall::perform() {
  // use remote values for fallback
  const char *name = this->parent_->get_name().c_str();
  if (this->publish_) {
    ESP_LOGD(TAG, "'%s' Setting:", name);
  }

  LightColorValues v = this->validate_();

  if (this->publish_) {
    // Only print state when it's being changed
    bool current_state = this->parent_->remote_values.is_on();
    if (this->state_.value_or(current_state) != current_state) {
      ESP_LOGD(TAG, "  State: %s", ONOFF(v.is_on()));
    }

    if (this->brightness_.has_value()) {
      ESP_LOGD(TAG, "  Brightness: %.0f%%", v.get_brightness() * 100.0f);
    }

    if (this->color_temperature_.has_value()) {
      ESP_LOGD(TAG, "  Color Temperature: %.1f mireds", v.get_color_temperature());
    }

    if (this->red_.has_value() || this->green_.has_value() || this->blue_.has_value()) {
      ESP_LOGD(TAG, "  Red=%.0f%%, Green=%.0f%%, Blue=%.0f%%", v.get_red() * 100.0f, v.get_green() * 100.0f,
               v.get_blue() * 100.0f);
    }
    if (this->white_.has_value()) {
      ESP_LOGD(TAG, "  White Value: %.0f%%", v.get_white() * 100.0f);
    }
  }

  if (this->has_flash_()) {
    // FLASH
    if (this->publish_) {
      ESP_LOGD(TAG, "  Flash Length: %.1fs", *this->flash_length_ / 1e3f);
    }

    this->parent_->start_flash_(v, *this->flash_length_);
  } else if (this->has_transition_()) {
    // TRANSITION
    if (this->publish_) {
      ESP_LOGD(TAG, "  Transition Length: %.1fs", *this->transition_length_ / 1e3f);
    }

    // Special case: Transition and effect can be set when turning off
    if (this->has_effect_()) {
      if (this->publish_) {
        ESP_LOGD(TAG, "  Effect: 'None'");
      }
      this->parent_->stop_effect_();
    }

    this->parent_->start_transition_(v, *this->transition_length_);

  } else if (this->has_effect_()) {
    // EFFECT
    auto effect = this->effect_;
    const char *effect_s;
    if (effect == 0)
      effect_s = "None";
    else
      effect_s = this->parent_->effects_[*this->effect_ - 1]->get_name().c_str();

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
  // use remote values for fallback
  auto *name = this->parent_->get_name().c_str();
  auto traits = this->parent_->get_traits();

  // Brightness exists check
  if (this->brightness_.has_value() && !traits.get_supports_brightness()) {
    ESP_LOGW(TAG, "'%s' - This light does not support setting brightness!", name);
    this->brightness_.reset();
  }

  // Transition length possible check
  if (this->transition_length_.has_value() && *this->transition_length_ != 0 && !traits.get_supports_brightness()) {
    ESP_LOGW(TAG, "'%s' - This light does not support transitions!", name);
    this->transition_length_.reset();
  }

  // Color brightness exists check
  if (this->color_brightness_.has_value() && !traits.get_supports_rgb()) {
    ESP_LOGW(TAG, "'%s' - This light does not support setting RGB brightness!", name);
    this->color_brightness_.reset();
  }

  // RGB exists check
  if (this->red_.has_value() || this->green_.has_value() || this->blue_.has_value()) {
    if (!traits.get_supports_rgb()) {
      ESP_LOGW(TAG, "'%s' - This light does not support setting RGB color!", name);
      this->red_.reset();
      this->green_.reset();
      this->blue_.reset();
    }
  }

  // White value exists check
  if (this->white_.has_value() && !traits.get_supports_rgb_white_value()) {
    ESP_LOGW(TAG, "'%s' - This light does not support setting white value!", name);
    this->white_.reset();
  }

  // Color temperature exists check
  if (this->color_temperature_.has_value() && !traits.get_supports_color_temperature()) {
    ESP_LOGW(TAG, "'%s' - This light does not support setting color temperature!", name);
    this->color_temperature_.reset();
  }

  // Handle interaction between RGB and white for color interlock
  if (traits.get_supports_color_interlock()) {
    // If white channel is specified, set color brightness to zero if it is enabled
    if (this->white_.has_value()) {
      if (*this->white_ > 0.0f) {
        if (this->color_brightness_.has_value() && *this->color_brightness_ > 0.0f) {
          ESP_LOGW(TAG, "'%s' - Cannot set color brightness and white value simultaneously with interlock!", name);
        }

        this->color_brightness_ = optional<float>(0.0f);
      }
    }
    // If only a color channel is specified, use white channel if it's white, otherwise disable white channel
    else if (this->red_.has_value() || this->green_.has_value() || this->blue_.has_value()) {
      if (*this->red_ == 1.0f && *this->green_ == 1.0f && *this->blue_ == 1.0f) {
        this->white_ = optional<float>(this->color_brightness_.has_value() ? *this->color_brightness_ : 1.0f);
        this->color_brightness_ = optional<float>(0.0f);
      } else {
        this->white_ = optional<float>(0.0f);
      }
    }
    // If only color brightness is set to non-zero, disable white channel
    else if (this->color_brightness_.has_value()) {
      if (*this->color_brightness_ > 0.0f) {
        this->white_ = optional<float>(0.0f);
      }
    }
  }

  // If only a color temperature is specified, change to white light
  if (this->color_temperature_.has_value() && !this->white_.has_value() && !this->red_.has_value() &&
      !this->green_.has_value() && !this->blue_.has_value()) {
    // Disable color LEDs explicitly if not already set
    if (traits.get_supports_rgb() && !this->color_brightness_.has_value())
      this->color_brightness_ = optional<float>(0.0f);

    this->red_ = optional<float>(1.0f);
    this->green_ = optional<float>(1.0f);
    this->blue_ = optional<float>(1.0f);

    // if setting color temperature from color (i.e. switching to white light), set White to 100%
    auto cv = this->parent_->remote_values;
    bool was_color = cv.get_red() != 1.0f || cv.get_blue() != 1.0f || cv.get_green() != 1.0f;
    if (traits.get_supports_color_interlock() || was_color) {
      this->white_ = optional<float>(1.0f);
    }
  }

#define VALIDATE_RANGE_(name_, upper_name) \
  if (name_##_.has_value()) { \
    auto val = *name_##_; \
    if (val < 0.0f || val > 1.0f) { \
      ESP_LOGW(TAG, "'%s' - %s value %.2f is out of range [0.0 - 1.0]!", name, upper_name, val); \
      name_##_ = clamp(val, 0.0f, 1.0f); \
    } \
  }
#define VALIDATE_RANGE(name, upper_name) VALIDATE_RANGE_(name, upper_name)

  // Range checks
  VALIDATE_RANGE(brightness, "Brightness")
  VALIDATE_RANGE(color_brightness, "Color brightness")
  VALIDATE_RANGE(red, "Red")
  VALIDATE_RANGE(green, "Green")
  VALIDATE_RANGE(blue, "Blue")
  VALIDATE_RANGE(white, "White")

  auto v = this->parent_->remote_values;
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

  v.normalize_color(traits);

  // Flash length check
  if (this->has_flash_() && *this->flash_length_ == 0) {
    ESP_LOGW(TAG, "'%s' - Flash length must be greater than zero!", name);
    this->flash_length_.reset();
  }

  // validate transition length/flash length/effect not used at the same time
  bool supports_transition = traits.get_supports_brightness();

  // If effect is already active, remove effect start
  if (this->has_effect_() && *this->effect_ == this->parent_->active_effect_index_) {
    this->effect_.reset();
  }

  // validate effect index
  if (this->has_effect_() && *this->effect_ > this->parent_->effects_.size()) {
    ESP_LOGW(TAG, "'%s' Invalid effect index %u", name, *this->effect_);
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
    } else if (this->parent_->active_effect_index_ != 0) {
      // Auto turn off effect
      this->effect_ = 0;
    }
  }

  // Disable saving for flashes
  if (this->has_flash_())
    this->save_ = false;

  return v;
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
  this->set_red_if_supported(values.get_red());
  this->set_green_if_supported(values.get_green());
  this->set_blue_if_supported(values.get_blue());
  this->set_white_if_supported(values.get_white());
  this->set_color_temperature_if_supported(values.get_color_temperature());
  return *this;
}
LightCall &LightCall::set_transition_length_if_supported(uint32_t transition_length) {
  if (this->parent_->get_traits().get_supports_brightness())
    this->set_transition_length(transition_length);
  return *this;
}
LightCall &LightCall::set_brightness_if_supported(float brightness) {
  if (this->parent_->get_traits().get_supports_brightness())
    this->set_brightness(brightness);
  return *this;
}
LightCall &LightCall::set_color_brightness_if_supported(float brightness) {
  if (this->parent_->get_traits().get_supports_rgb_white_value())
    this->set_brightness(brightness);
  return *this;
}
LightCall &LightCall::set_red_if_supported(float red) {
  if (this->parent_->get_traits().get_supports_rgb())
    this->set_red(red);
  return *this;
}
LightCall &LightCall::set_green_if_supported(float green) {
  if (this->parent_->get_traits().get_supports_rgb())
    this->set_green(green);
  return *this;
}
LightCall &LightCall::set_blue_if_supported(float blue) {
  if (this->parent_->get_traits().get_supports_rgb())
    this->set_blue(blue);
  return *this;
}
LightCall &LightCall::set_white_if_supported(float white) {
  if (this->parent_->get_traits().get_supports_rgb_white_value())
    this->set_white(white);
  return *this;
}
LightCall &LightCall::set_color_temperature_if_supported(float color_temperature) {
  if (this->parent_->get_traits().get_supports_color_temperature())
    this->set_color_temperature(color_temperature);
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
