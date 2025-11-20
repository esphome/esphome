#include "addressable_light.h"
#include "esphome/core/log.h"

namespace esphome::light {

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

inline constexpr uint8_t subtract_scaled_difference(uint8_t a, uint8_t b, int32_t scale) {
  return uint8_t(int32_t(a) - (((int32_t(a) - int32_t(b)) * scale) / 256));
}

optional<LightColorValues> AddressableLightTransformer::apply() {
  float smoothed_progress = LightTransformer::smoothed_progress(this->get_progress_());

  // When running an output-buffer modifying effect, don't try to transition individual LEDs, but instead just fade the
  // LightColorValues. write_state() then picks up the change in brightness, and the color change is picked up by the
  // effects which respect it.
  if (this->light_.is_effect_active())
    return LightColorValues::lerp(this->get_start_values(), this->get_target_values(), smoothed_progress);

  // Use a specialized transition for addressable lights: instead of using a unified transition for
  // all LEDs, we use the current state of each LED as the start.

  // We can't use a direct lerp smoothing here though - that would require creating a copy of the original
  // state of each LED at the start of the transition. Instead, we "fake" the look of lerp by calculating
  // the delta between the current state and the target state, assuming that the delta represents the rest
  // of the transition that was to be applied as of the previous transition step, and scaling the delta for
  // what should be left after the current transition step. In this manner, the delta decays to zero as the
  // transition progresses.
  //
  // Here's an example of how the algorithm progresses in discrete steps:
  //
  // At time = 0.00, 0% complete, 100% remaining, 100% will remain after this step, so the scale is 100% / 100% = 100%.
  // At time = 0.10, 0% complete, 100% remaining, 90% will remain after this step, so the scale is 90% / 100% = 90%.
  // At time = 0.20, 10% complete, 90% remaining, 80% will remain after this step, so the scale is 80% / 90% = 88.9%.
  // At time = 0.50, 20% complete, 80% remaining, 50% will remain after this step, so the scale is 50% / 80% = 62.5%.
  // At time = 0.90, 50% complete, 50% remaining, 10% will remain after this step, so the scale is 10% / 50% = 20%.
  // At time = 0.91, 90% complete, 10% remaining, 9% will remain after this step, so the scale is 9% / 10% = 90%.
  // At time = 1.00, 91% complete, 9% remaining, 0% will remain after this step, so the scale is 0% / 9% = 0%.
  //
  // Because the color values are quantized to 8 bit resolution after each step, the transition may appear
  // non-linear when applying small deltas.

  if (smoothed_progress > this->last_transition_progress_ && this->last_transition_progress_ < 1.f) {
    int32_t scale = int32_t(256.f * std::max((1.f - smoothed_progress) / (1.f - this->last_transition_progress_), 0.f));
    for (auto led : this->light_) {
      led.set_rgbw(subtract_scaled_difference(this->target_color_.red, led.get_red(), scale),
                   subtract_scaled_difference(this->target_color_.green, led.get_green(), scale),
                   subtract_scaled_difference(this->target_color_.blue, led.get_blue(), scale),
                   subtract_scaled_difference(this->target_color_.white, led.get_white(), scale));
    }
    this->last_transition_progress_ = smoothed_progress;
    this->light_.schedule_show();
  }

  return {};
}

}  // namespace esphome::light
