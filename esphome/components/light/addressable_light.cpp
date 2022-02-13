#include "addressable_light.h"
#include "esphome/core/log.h"

namespace esphome {
namespace light {

static const char *const TAG = "light.addressable";

void AddressableLight::call_setup() {
  this->setup();

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  this->set_interval(5000, [this]() {
    const char *name = this->state_parent_ == nullptr ? "" : this->state_parent_->get_name().c_str();
    ESP_LOGVV(TAG, "Addressable Light '%s' (effect_active=%s)", name, YESNO(this->effect_active_));
    for (int i = 0; i < this->size(); i++) {
      auto color = this->get(i);
      ESP_LOGVV(TAG, "  [%2d] Color: R=%3u G=%3u B=%3u W=%3u", i, color.get_red_raw(), color.get_green_raw(),
                color.get_blue_raw(), color.get_white_raw());
    }
    ESP_LOGVV(TAG, " ");
  });
#endif
}

std::unique_ptr<LightTransformer> AddressableLight::create_default_transition() {
  return make_unique<AddressableLightTransformer>(*this);
}

Color color_from_light_color_values(LightColorValues val) {
  auto r = to_uint8_scale(val.get_color_brightness() * val.get_red());
  auto g = to_uint8_scale(val.get_color_brightness() * val.get_green());
  auto b = to_uint8_scale(val.get_color_brightness() * val.get_blue());
  auto w = to_uint8_scale(val.get_white());
  return Color(r, g, b, w);
}

void AddressableLight::update_state(LightState *state) {
  auto val = state->current_values;
  auto max_brightness = to_uint8_scale(val.get_brightness() * val.get_state());
  this->correction_.set_local_brightness(max_brightness);

  if (this->is_effect_active())
    return;

  // don't use LightState helper, gamma correction+brightness is handled by ESPColorView
  this->all() = color_from_light_color_values(val);
  this->schedule_show();
}

void AddressableLightTransformer::start() {
  // don't try to transition over running effects.
  if (this->light_.is_effect_active())
    return;

  auto end_values = this->target_values_;
  this->target_color_ = color_from_light_color_values(end_values);

  // our transition will handle brightness, disable brightness in correction.
  this->light_.correction_.set_local_brightness(255);
  this->target_color_ *= to_uint8_scale(end_values.get_brightness() * end_values.get_state());
}

optional<LightColorValues> AddressableLightTransformer::apply() {
  float smoothed_progress = LightTransitionTransformer::smoothed_progress(this->get_progress_());

  // When running an output-buffer modifying effect, don't try to transition individual LEDs, but instead just fade the
  // LightColorValues. write_state() then picks up the change in brightness, and the color change is picked up by the
  // effects which respect it.
  if (this->light_.is_effect_active())
    return LightColorValues::lerp(this->get_start_values(), this->get_target_values(), smoothed_progress);

  // Use a specialized transition for addressable lights: instead of using a unified transition for
  // all LEDs, we use the current state of each LED as the start.

  // We can't use a direct lerp smoothing here though - that would require creating a copy of the original
  // state of each LED at the start of the transition.
  // Instead, we "fake" the look of the LERP by using an exponential average over time and using
  // dynamically-calculated alpha values to match the look.

  float denom = (1.0f - smoothed_progress);
  float alpha = denom == 0.0f ? 1.0f : (smoothed_progress - this->last_transition_progress_) / denom;

  // We need to use a low-resolution alpha here which makes the transition set in only after ~half of the length
  // We solve this by accumulating the fractional part of the alpha over time.
  float alpha255 = alpha * 255.0f;
  float alpha255int = floorf(alpha255);
  float alpha255remainder = alpha255 - alpha255int;

  this->accumulated_alpha_ += alpha255remainder;
  float alpha_add = floorf(this->accumulated_alpha_);
  this->accumulated_alpha_ -= alpha_add;

  alpha255 += alpha_add;
  alpha255 = clamp(alpha255, 0.0f, 255.0f);
  auto alpha8 = static_cast<uint8_t>(alpha255);

  if (alpha8 != 0) {
    uint8_t inv_alpha8 = 255 - alpha8;
    Color add = this->target_color_ * alpha8;

    for (auto led : this->light_)
      led.set(add + led.get() * inv_alpha8);
  }

  this->last_transition_progress_ = smoothed_progress;
  this->light_.schedule_show();

  return {};
}

}  // namespace light
}  // namespace esphome
