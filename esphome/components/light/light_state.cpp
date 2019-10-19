#include "light_state.h"
#include "light_output.h"
#include "esphome/core/log.h"

namespace esphome {
namespace light {

static const char *TAG = "light";

void LightState::start_transition_(const LightColorValues &target, uint32_t length) {
  this->transformer_ = make_unique<LightTransitionTransformer>(millis(), length, this->current_values, target);
  this->remote_values = this->transformer_->get_remote_values();
}

void LightState::start_flash_(const LightColorValues &target, uint32_t length) {
  LightColorValues end_colors = this->current_values;
  // If starting a flash if one is already happening, set end values to end values of current flash
  // Hacky but works
  if (this->transformer_ != nullptr)
    end_colors = this->transformer_->get_end_values();
  this->transformer_ = make_unique<LightFlashTransformer>(millis(), length, end_colors, target);
  this->remote_values = this->transformer_->get_remote_values();
}

LightState::LightState(const std::string &name, LightOutput *output) : Nameable(name), output_(output) {}

void LightState::set_immediately_(const LightColorValues &target, bool set_remote_values) {
  this->transformer_ = nullptr;
  this->current_values = target;
  if (set_remote_values) {
    this->remote_values = target;
  }
  this->next_write_ = true;
}

LightColorValues LightState::get_current_values() { return this->current_values; }

void LightState::publish_state() {
  this->remote_values_callback_.call();
  this->next_write_ = true;
}

LightColorValues LightState::get_remote_values() { return this->remote_values; }

std::string LightState::get_effect_name() {
  if (this->active_effect_index_ > 0)
    return this->effects_[this->active_effect_index_ - 1]->get_name();
  else
    return "None";
}

void LightState::start_effect_(uint32_t effect_index) {
  this->stop_effect_();
  if (effect_index == 0)
    return;

  this->active_effect_index_ = effect_index;
  auto *effect = this->get_active_effect_();
  effect->start_internal();
}

bool LightState::supports_effects() { return !this->effects_.empty(); }
void LightState::set_transformer_(std::unique_ptr<LightTransformer> transformer) {
  this->transformer_ = std::move(transformer);
}
void LightState::stop_effect_() {
  auto *effect = this->get_active_effect_();
  if (effect != nullptr) {
    effect->stop();
  }
  this->active_effect_index_ = 0;
}

void LightState::set_default_transition_length(uint32_t default_transition_length) {
  this->default_transition_length_ = default_transition_length;
}
#ifdef USE_JSON
void LightState::dump_json(JsonObject &root) {
  if (this->supports_effects())
    root["effect"] = this->get_effect_name();
  this->remote_values.dump_json(root, this->output_->get_traits());
}
#endif

struct LightStateRTCState {
  bool state{false};
  float brightness{1.0f};
  float red{1.0f};
  float green{1.0f};
  float blue{1.0f};
  float white{1.0f};
  float color_temp{1.0f};
  uint32_t effect{0};
};

void LightState::setup() {
  ESP_LOGCONFIG(TAG, "Setting up light '%s'...", this->get_name().c_str());

  this->output_->setup_state(this);
  for (auto *effect : this->effects_) {
    effect->init_internal(this);
  }

  auto call = this->make_call();
  LightStateRTCState recovered{};
  switch (this->restore_mode_) {
    case LIGHT_RESTORE_DEFAULT_OFF:
    case LIGHT_RESTORE_DEFAULT_ON:
      this->rtc_ = global_preferences.make_preference<LightStateRTCState>(this->get_object_id_hash());
      // Attempt to load from preferences, else fall back to default values from struct
      if (!this->rtc_.load(&recovered)) {
        recovered.state = this->restore_mode_ == LIGHT_RESTORE_DEFAULT_ON;
      }
      break;
    case LIGHT_ALWAYS_OFF:
      recovered.state = false;
      break;
    case LIGHT_ALWAYS_ON:
      recovered.state = true;
      break;
  }

  call.set_state(recovered.state);
  call.set_brightness_if_supported(recovered.brightness);
  call.set_red_if_supported(recovered.red);
  call.set_green_if_supported(recovered.green);
  call.set_blue_if_supported(recovered.blue);
  call.set_white_if_supported(recovered.white);
  call.set_color_temperature_if_supported(recovered.color_temp);
  if (recovered.effect != 0) {
    call.set_effect(recovered.effect);
  } else {
    call.set_transition_length_if_supported(0);
  }
  call.perform();
}
void LightState::loop() {
  // Apply effect (if any)
  auto *effect = this->get_active_effect_();
  if (effect != nullptr) {
    effect->apply();
  }

  // Apply transformer (if any)
  if (this->transformer_ != nullptr) {
    if (this->transformer_->is_finished()) {
      this->remote_values = this->current_values = this->transformer_->get_end_values();
      if (this->transformer_->publish_at_end())
        this->publish_state();
      this->transformer_ = nullptr;
    } else {
      this->current_values = this->transformer_->get_values();
      this->remote_values = this->transformer_->get_remote_values();
    }
    this->next_write_ = true;
  }

  if (this->next_write_) {
    this->output_->write_state(this);
    this->next_write_ = false;
  }
}
LightTraits LightState::get_traits() { return this->output_->get_traits(); }
const std::vector<LightEffect *> &LightState::get_effects() const { return this->effects_; }
void LightState::add_effects(const std::vector<LightEffect *> effects) {
  this->effects_.reserve(this->effects_.size() + effects.size());
  for (auto *effect : effects) {
    this->effects_.push_back(effect);
  }
}
LightCall LightState::turn_on() { return this->make_call().set_state(true); }
LightCall LightState::turn_off() { return this->make_call().set_state(false); }
LightCall LightState::toggle() { return this->make_call().set_state(!this->remote_values.is_on()); }
LightCall LightState::make_call() { return LightCall(this); }
uint32_t LightState::hash_base() { return 1114400283; }
void LightState::dump_config() {
  ESP_LOGCONFIG(TAG, "Light '%s'", this->get_name().c_str());
  if (this->get_traits().get_supports_brightness()) {
    ESP_LOGCONFIG(TAG, "  Default Transition Length: %.1fs", this->default_transition_length_ / 1e3f);
    ESP_LOGCONFIG(TAG, "  Gamma Correct: %.2f", this->gamma_correct_);
  }
  if (this->get_traits().get_supports_color_temperature()) {
    ESP_LOGCONFIG(TAG, "  Min Mireds: %.1f", this->get_traits().get_min_mireds());
    ESP_LOGCONFIG(TAG, "  Max Mireds: %.1f", this->get_traits().get_max_mireds());
  }
}
#ifdef USE_MQTT_LIGHT
MQTTJSONLightComponent *LightState::get_mqtt() const { return this->mqtt_; }
void LightState::set_mqtt(MQTTJSONLightComponent *mqtt) { this->mqtt_ = mqtt; }
#endif

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

  if (this->publish_) {
    this->parent_->publish_state();
  }

  if (this->save_) {
    LightStateRTCState saved;
    saved.state = v.is_on();
    saved.brightness = v.get_brightness();
    saved.red = v.get_red();
    saved.green = v.get_green();
    saved.blue = v.get_blue();
    saved.white = v.get_white();
    saved.color_temp = v.get_color_temperature();
    saved.effect = this->parent_->active_effect_index_;
    this->parent_->rtc_.save(&saved);
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
  VALIDATE_RANGE(red, "Red")
  VALIDATE_RANGE(green, "Green")
  VALIDATE_RANGE(blue, "Blue")
  VALIDATE_RANGE(white, "White")

  auto v = this->parent_->remote_values;
  if (this->state_.has_value())
    v.set_state(*this->state_);
  if (this->brightness_.has_value())
    v.set_brightness(*this->brightness_);

  if (this->brightness_.has_value())
    v.set_brightness(*this->brightness_);
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

float LightState::get_setup_priority() const { return setup_priority::HARDWARE - 1.0f; }
LightOutput *LightState::get_output() const { return this->output_; }
void LightState::set_gamma_correct(float gamma_correct) { this->gamma_correct_ = gamma_correct; }
void LightState::current_values_as_binary(bool *binary) { this->current_values.as_binary(binary); }
void LightState::current_values_as_brightness(float *brightness) {
  this->current_values.as_brightness(brightness);
  *brightness = gamma_correct(*brightness, this->gamma_correct_);
}
void LightState::current_values_as_rgb(float *red, float *green, float *blue) {
  this->current_values.as_rgb(red, green, blue);
  *red = gamma_correct(*red, this->gamma_correct_);
  *green = gamma_correct(*green, this->gamma_correct_);
  *blue = gamma_correct(*blue, this->gamma_correct_);
}
void LightState::current_values_as_rgbw(float *red, float *green, float *blue, float *white) {
  this->current_values.as_rgbw(red, green, blue, white);
  *red = gamma_correct(*red, this->gamma_correct_);
  *green = gamma_correct(*green, this->gamma_correct_);
  *blue = gamma_correct(*blue, this->gamma_correct_);
  *white = gamma_correct(*white, this->gamma_correct_);
}
void LightState::current_values_as_rgbww(float *red, float *green, float *blue, float *cold_white, float *warm_white) {
  auto traits = this->get_traits();
  this->current_values.as_rgbww(traits.get_min_mireds(), traits.get_max_mireds(), red, green, blue, cold_white,
                                warm_white);
  *red = gamma_correct(*red, this->gamma_correct_);
  *green = gamma_correct(*green, this->gamma_correct_);
  *blue = gamma_correct(*blue, this->gamma_correct_);
  *cold_white = gamma_correct(*cold_white, this->gamma_correct_);
  *warm_white = gamma_correct(*warm_white, this->gamma_correct_);
}
void LightState::current_values_as_cwww(float *cold_white, float *warm_white) {
  auto traits = this->get_traits();
  this->current_values.as_cwww(traits.get_min_mireds(), traits.get_max_mireds(), cold_white, warm_white);
  *cold_white = gamma_correct(*cold_white, this->gamma_correct_);
  *warm_white = gamma_correct(*warm_white, this->gamma_correct_);
}
void LightState::add_new_remote_values_callback(std::function<void()> &&send_callback) {
  this->remote_values_callback_.add(std::move(send_callback));
}
LightEffect *LightState::get_active_effect_() {
  if (this->active_effect_index_ == 0)
    return nullptr;
  else
    return this->effects_[this->active_effect_index_ - 1];
}

}  // namespace light
}  // namespace esphome
