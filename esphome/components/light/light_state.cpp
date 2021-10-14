#include "esphome/core/log.h"
#include "light_state.h"
#include "light_output.h"
#include "transformers.h"

namespace esphome {
namespace light {

static const char *const TAG = "light";

LightState::LightState(const std::string &name, LightOutput *output) : EntityBase(name), output_(output) {}
LightState::LightState(LightOutput *output) : output_(output) {}

LightTraits LightState::get_traits() { return this->output_->get_traits(); }
LightCall LightState::turn_on() { return this->make_call().set_state(true); }
LightCall LightState::turn_off() { return this->make_call().set_state(false); }
LightCall LightState::toggle() { return this->make_call().set_state(!this->remote_values.is_on()); }
LightCall LightState::make_call() { return LightCall(this); }

struct LightStateRTCState {
  ColorMode color_mode{ColorMode::UNKNOWN};
  bool state{false};
  float brightness{1.0f};
  float color_brightness{1.0f};
  float red{1.0f};
  float green{1.0f};
  float blue{1.0f};
  float white{1.0f};
  float color_temp{1.0f};
  float cold_white{1.0f};
  float warm_white{1.0f};
  uint32_t effect{0};
};

void LightState::setup() {
  ESP_LOGCONFIG(TAG, "Setting up light '%s'...", this->get_name().c_str());

  this->output_->setup_state(this);
  for (auto *effect : this->effects_) {
    effect->init_internal(this);
  }

  // When supported color temperature range is known, initialize color temperature setting within bounds.
  float min_mireds = this->get_traits().get_min_mireds();
  if (min_mireds > 0) {
    this->remote_values.set_color_temperature(min_mireds);
    this->current_values.set_color_temperature(min_mireds);
  }

  auto call = this->make_call();
  LightStateRTCState recovered{};
  switch (this->restore_mode_) {
    case LIGHT_RESTORE_DEFAULT_OFF:
    case LIGHT_RESTORE_DEFAULT_ON:
    case LIGHT_RESTORE_INVERTED_DEFAULT_OFF:
    case LIGHT_RESTORE_INVERTED_DEFAULT_ON:
      this->rtc_ = global_preferences->make_preference<LightStateRTCState>(this->get_object_id_hash());
      // Attempt to load from preferences, else fall back to default values
      if (!this->rtc_.load(&recovered)) {
        recovered.state = false;
        if (this->restore_mode_ == LIGHT_RESTORE_DEFAULT_ON ||
            this->restore_mode_ == LIGHT_RESTORE_INVERTED_DEFAULT_ON) {
          recovered.state = true;
        }
      } else if (this->restore_mode_ == LIGHT_RESTORE_INVERTED_DEFAULT_OFF ||
                 this->restore_mode_ == LIGHT_RESTORE_INVERTED_DEFAULT_ON) {
        // Inverted restore state
        recovered.state = !recovered.state;
      }
      break;
    case LIGHT_ALWAYS_OFF:
      recovered.state = false;
      break;
    case LIGHT_ALWAYS_ON:
      recovered.state = true;
      break;
  }

  call.set_color_mode_if_supported(recovered.color_mode);
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
  if (this->get_traits().supports_color_capability(ColorCapability::BRIGHTNESS)) {
    ESP_LOGCONFIG(TAG, "  Default Transition Length: %.1fs", this->default_transition_length_ / 1e3f);
    ESP_LOGCONFIG(TAG, "  Gamma Correct: %.2f", this->gamma_correct_);
  }
  if (this->get_traits().supports_color_capability(ColorCapability::COLOR_TEMPERATURE)) {
    ESP_LOGCONFIG(TAG, "  Min Mireds: %.1f", this->get_traits().get_min_mireds());
    ESP_LOGCONFIG(TAG, "  Max Mireds: %.1f", this->get_traits().get_max_mireds());
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
    if (values.has_value()) {
      this->current_values = *values;
      this->output_->update_state(this);
      this->next_write_ = true;
    }

    if (this->transformer_->is_finished()) {
      // if the transition has written directly to the output, current_values is outdated, so update it
      this->current_values = this->transformer_->get_target_values();

      this->transformer_->stop();
      this->transformer_ = nullptr;
      this->target_state_reached_callback_.call();
    }
  }

  // Write state to the light
  if (this->next_write_) {
    this->next_write_ = false;
    this->output_->write_state(this);
  }
}

float LightState::get_setup_priority() const { return setup_priority::HARDWARE - 1.0f; }
uint32_t LightState::hash_base() { return 1114400283; }

void LightState::publish_state() { this->remote_values_callback_.call(); }

LightOutput *LightState::get_output() const { return this->output_; }
std::string LightState::get_effect_name() {
  if (this->active_effect_index_ > 0)
    return this->effects_[this->active_effect_index_ - 1]->get_name();
  else
    return "None";
}

void LightState::add_new_remote_values_callback(std::function<void()> &&send_callback) {
  this->remote_values_callback_.add(std::move(send_callback));
}
void LightState::add_new_target_state_reached_callback(std::function<void()> &&send_callback) {
  this->target_state_reached_callback_.add(std::move(send_callback));
}

void LightState::set_default_transition_length(uint32_t default_transition_length) {
  this->default_transition_length_ = default_transition_length;
}
uint32_t LightState::get_default_transition_length() const { return this->default_transition_length_; }
void LightState::set_flash_transition_length(uint32_t flash_transition_length) {
  this->flash_transition_length_ = flash_transition_length;
}
uint32_t LightState::get_flash_transition_length() const { return this->flash_transition_length_; }
void LightState::set_gamma_correct(float gamma_correct) { this->gamma_correct_ = gamma_correct; }
void LightState::set_restore_mode(LightRestoreMode restore_mode) { this->restore_mode_ = restore_mode; }
bool LightState::supports_effects() { return !this->effects_.empty(); }
const std::vector<LightEffect *> &LightState::get_effects() const { return this->effects_; }
void LightState::add_effects(const std::vector<LightEffect *> &effects) {
  this->effects_.reserve(this->effects_.size() + effects.size());
  for (auto *effect : effects) {
    this->effects_.push_back(effect);
  }
}

void LightState::current_values_as_binary(bool *binary) { this->current_values.as_binary(binary); }
void LightState::current_values_as_brightness(float *brightness) {
  this->current_values.as_brightness(brightness, this->gamma_correct_);
}
void LightState::current_values_as_rgb(float *red, float *green, float *blue, bool color_interlock) {
  auto traits = this->get_traits();
  this->current_values.as_rgb(red, green, blue, this->gamma_correct_, false);
}
void LightState::current_values_as_rgbw(float *red, float *green, float *blue, float *white, bool color_interlock) {
  auto traits = this->get_traits();
  this->current_values.as_rgbw(red, green, blue, white, this->gamma_correct_, false);
}
void LightState::current_values_as_rgbww(float *red, float *green, float *blue, float *cold_white, float *warm_white,
                                         bool constant_brightness) {
  this->current_values.as_rgbww(red, green, blue, cold_white, warm_white, this->gamma_correct_, constant_brightness);
}
void LightState::current_values_as_rgbct(float *red, float *green, float *blue, float *color_temperature,
                                         float *white_brightness) {
  auto traits = this->get_traits();
  this->current_values.as_rgbct(traits.get_min_mireds(), traits.get_max_mireds(), red, green, blue, color_temperature,
                                white_brightness, this->gamma_correct_);
}
void LightState::current_values_as_cwww(float *cold_white, float *warm_white, bool constant_brightness) {
  auto traits = this->get_traits();
  this->current_values.as_cwww(cold_white, warm_white, this->gamma_correct_, constant_brightness);
}
void LightState::current_values_as_ct(float *color_temperature, float *white_brightness) {
  auto traits = this->get_traits();
  this->current_values.as_ct(traits.get_min_mireds(), traits.get_max_mireds(), color_temperature, white_brightness,
                             this->gamma_correct_);
}

void LightState::start_effect_(uint32_t effect_index) {
  this->stop_effect_();
  if (effect_index == 0)
    return;

  this->active_effect_index_ = effect_index;
  auto *effect = this->get_active_effect_();
  effect->start_internal();
}
LightEffect *LightState::get_active_effect_() {
  if (this->active_effect_index_ == 0)
    return nullptr;
  else
    return this->effects_[this->active_effect_index_ - 1];
}
void LightState::stop_effect_() {
  auto *effect = this->get_active_effect_();
  if (effect != nullptr) {
    effect->stop();
  }
  this->active_effect_index_ = 0;
}

void LightState::start_transition_(const LightColorValues &target, uint32_t length, bool set_remote_values) {
  this->transformer_ = this->output_->create_default_transition();
  this->transformer_->setup(this->current_values, target, length);

  if (set_remote_values) {
    this->remote_values = target;
  }
}

void LightState::start_flash_(const LightColorValues &target, uint32_t length, bool set_remote_values) {
  LightColorValues end_colors = this->remote_values;
  // If starting a flash if one is already happening, set end values to end values of current flash
  // Hacky but works
  if (this->transformer_ != nullptr)
    end_colors = this->transformer_->get_start_values();

  this->transformer_ = make_unique<LightFlashTransformer>(*this);
  this->transformer_->setup(end_colors, target, length);

  if (set_remote_values) {
    this->remote_values = target;
  };
}

void LightState::set_immediately_(const LightColorValues &target, bool set_remote_values) {
  this->transformer_ = nullptr;
  this->current_values = target;
  if (set_remote_values) {
    this->remote_values = target;
  }
  this->output_->update_state(this);
  this->next_write_ = true;
}

void LightState::save_remote_values_() {
  LightStateRTCState saved;
  saved.color_mode = this->remote_values.get_color_mode();
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
  saved.effect = this->active_effect_index_;
  this->rtc_.save(&saved);
}

}  // namespace light
}  // namespace esphome
