#include "addressable_light.h"
#include "esphome/core/log.h"

namespace esphome {
namespace light {

static const char *TAG = "light.addressable";

const ESPColor ESPColor::BLACK = ESPColor(0, 0, 0, 0);
const ESPColor ESPColor::WHITE = ESPColor(255, 255, 255, 255);

ESPColor ESPHSVColor::to_rgb() const {
  // based on FastLED's hsv rainbow to rgb
  const uint8_t hue = this->hue;
  const uint8_t sat = this->saturation;
  const uint8_t val = this->value;
  // upper 3 hue bits are for branch selection, lower 5 are for values
  const uint8_t offset8 = (hue & 0x1F) << 3;  // 0..248
  // third of the offset, 255/3 = 85 (actually only up to 82; 164)
  const uint8_t third = esp_scale8(offset8, 85);
  const uint8_t two_thirds = esp_scale8(offset8, 170);
  ESPColor rgb(255, 255, 255, 0);
  switch (hue >> 5) {
    case 0b000:
      rgb.r = 255 - third;
      rgb.g = third;
      rgb.b = 0;
      break;
    case 0b001:
      rgb.r = 171;
      rgb.g = 85 + third;
      rgb.b = 0;
      break;
    case 0b010:
      rgb.r = 171 - two_thirds;
      rgb.g = 170 + third;
      rgb.b = 0;
      break;
    case 0b011:
      rgb.r = 0;
      rgb.g = 255 - third;
      rgb.b = third;
      break;
    case 0b100:
      rgb.r = 0;
      rgb.g = 171 - two_thirds;
      rgb.b = 85 + two_thirds;
      break;
    case 0b101:
      rgb.r = third;
      rgb.g = 0;
      rgb.b = 255 - third;
      break;
    case 0b110:
      rgb.r = 85 + third;
      rgb.g = 0;
      rgb.b = 171 - third;
      break;
    case 0b111:
      rgb.r = 170 + third;
      rgb.g = 0;
      rgb.b = 85 - third;
      break;
    default:
      break;
  }
  // low saturation -> add uniform color to orig. hue
  // high saturation -> use hue directly
  // scales with square of saturation
  // (r,g,b) = (r,g,b) * sat + (1 - sat)^2
  rgb *= sat;
  const uint8_t desat = 255 - sat;
  rgb += esp_scale8(desat, desat);
  // (r,g,b) = (r,g,b) * val
  rgb *= val;
  return rgb;
}

void ESPRangeView::set(const ESPColor &color) {
  for (int32_t i = this->begin_; i < this->end_; i++) {
    (*this->parent_)[i] = color;
  }
}
ESPColorView ESPRangeView::operator[](int32_t index) const {
  index = interpret_index(index, this->size()) + this->begin_;
  return (*this->parent_)[index];
}
ESPRangeIterator ESPRangeView::begin() { return {*this, this->begin_}; }
ESPRangeIterator ESPRangeView::end() { return {*this, this->end_}; }
void ESPRangeView::set_red(uint8_t red) {
  for (auto c : *this)
    c.set_red(red);
}
void ESPRangeView::set_green(uint8_t green) {
  for (auto c : *this)
    c.set_green(green);
}
void ESPRangeView::set_blue(uint8_t blue) {
  for (auto c : *this)
    c.set_blue(blue);
}
void ESPRangeView::set_white(uint8_t white) {
  for (auto c : *this)
    c.set_white(white);
}
void ESPRangeView::set_effect_data(uint8_t effect_data) {
  for (auto c : *this)
    c.set_effect_data(effect_data);
}
void ESPRangeView::fade_to_white(uint8_t amnt) {
  for (auto c : *this)
    c.fade_to_white(amnt);
}
void ESPRangeView::fade_to_black(uint8_t amnt) {
  for (auto c : *this)
    c.fade_to_black(amnt);
}
void ESPRangeView::lighten(uint8_t delta) {
  for (auto c : *this)
    c.lighten(delta);
}
void ESPRangeView::darken(uint8_t delta) {
  for (auto c : *this)
    c.darken(delta);
}
ESPRangeView &ESPRangeView::operator=(const ESPRangeView &rhs) {
  // If size doesn't match, error (todo warning)
  if (rhs.size() != this->size())
    return *this;

  if (this->parent_ != rhs.parent_) {
    for (int32_t i = 0; i < this->size(); i++)
      (*this)[i].set(rhs[i].get());
    return *this;
  }

  // If both equal, already done
  if (rhs.begin_ == this->begin_)
    return *this;

  if (rhs.begin_ > this->begin_) {
    // Copy from left
    for (int32_t i = 0; i < this->size(); i++) {
      (*this)[i].set(rhs[i].get());
    }
  } else {
    // Copy from right
    for (int32_t i = this->size() - 1; i >= 0; i--) {
      (*this)[i].set(rhs[i].get());
    }
  }

  return *this;
}

ESPColorView ESPRangeIterator::operator*() const { return this->range_.parent_->get(this->i_); }

int32_t HOT interpret_index(int32_t index, int32_t size) {
  if (index < 0)
    return size + index;
  return index;
}

void AddressableLight::call_setup() {
  this->setup();

#ifdef ESPHOME_LOG_HAS_VERY_VERBOSE
  this->set_interval(5000, [this]() {
    const char *name = this->state_parent_ == nullptr ? "" : this->state_parent_->get_name().c_str();
    ESP_LOGVV(TAG, "Addressable Light '%s' (effect_active=%s next_show=%s)", name, YESNO(this->effect_active_),
              YESNO(this->next_show_));
    for (int i = 0; i < this->size(); i++) {
      auto color = this->get(i);
      ESP_LOGVV(TAG, "  [%2d] Color: R=%3u G=%3u B=%3u W=%3u", i, color.get_red_raw(), color.get_green_raw(),
                color.get_blue_raw(), color.get_white_raw());
    }
    ESP_LOGVV(TAG, "");
  });
#endif
}

ESPColor esp_color_from_light_color_values(LightColorValues val) {
  auto r = static_cast<uint8_t>(roundf(val.get_red() * 255.0f));
  auto g = static_cast<uint8_t>(roundf(val.get_green() * 255.0f));
  auto b = static_cast<uint8_t>(roundf(val.get_blue() * 255.0f));
  auto w = static_cast<uint8_t>(roundf(val.get_white() * val.get_state() * 255.0f));
  return ESPColor(r, g, b, w);
}

void AddressableLight::write_state(LightState *state) {
  auto val = state->current_values;
  auto max_brightness = static_cast<uint8_t>(roundf(val.get_brightness() * val.get_state() * 255.0f));
  this->correction_.set_local_brightness(max_brightness);

  this->last_transition_progress_ = 0.0f;
  this->accumulated_alpha_ = 0.0f;

  if (this->is_effect_active())
    return;

  // don't use LightState helper, gamma correction+brightness is handled by ESPColorView

  if (state->transformer_ == nullptr || !state->transformer_->is_transition()) {
    // no transformer active or non-transition one
    this->all() = esp_color_from_light_color_values(val);
  } else {
    // transition transformer active, activate specialized transition for addressable effects
    // instead of using a unified transition for all LEDs, we use the current state each LED as the
    // start. Warning: ugly

    // We can't use a direct lerp smoothing here though - that would require creating a copy of the original
    // state of each LED at the start of the transition
    // Instead, we "fake" the look of the LERP by using an exponential average over time and using
    // dynamically-calculated alpha values to match the look of the

    float new_progress = state->transformer_->get_progress();
    float prev_smoothed = LightTransitionTransformer::smoothed_progress(last_transition_progress_);
    float new_smoothed = LightTransitionTransformer::smoothed_progress(new_progress);
    this->last_transition_progress_ = new_progress;

    auto end_values = state->transformer_->get_end_values();
    ESPColor target_color = esp_color_from_light_color_values(end_values);

    // our transition will handle brightness, disable brightness in correction.
    this->correction_.set_local_brightness(255);
    uint8_t orig_w = target_color.w;
    target_color *= static_cast<uint8_t>(roundf(end_values.get_brightness() * end_values.get_state() * 255.0f));
    // w is not scaled by brightness
    target_color.w = orig_w;

    float denom = (1.0f - new_smoothed);
    float alpha = denom == 0.0f ? 0.0f : (new_smoothed - prev_smoothed) / denom;

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
      ESPColor add = target_color * alpha8;

      for (auto led : *this)
        led = add + led.get() * inv_alpha8;
    }
  }

  this->schedule_show();
}

void ESPColorCorrection::calculate_gamma_table(float gamma) {
  for (uint16_t i = 0; i < 256; i++) {
    // corrected = val ^ gamma
    auto corrected = static_cast<uint8_t>(roundf(255.0f * gamma_correct(i / 255.0f, gamma)));
    this->gamma_table_[i] = corrected;
  }
  if (gamma == 0.0f) {
    for (uint16_t i = 0; i < 256; i++)
      this->gamma_reverse_table_[i] = i;
    return;
  }
  for (uint16_t i = 0; i < 256; i++) {
    // val = corrected ^ (1/gamma)
    auto uncorrected = static_cast<uint8_t>(roundf(255.0f * powf(i / 255.0f, 1.0f / gamma)));
    this->gamma_reverse_table_[i] = uncorrected;
  }
}

}  // namespace light
}  // namespace esphome
