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

/* The default fade transition for non-addressable lights from state A to state B is given by
 *   f(p) = lerp(A, B, p) = (1-p)*A + p*B
 * where p = 0..1 is the progress of the transition.
 *
 * For the addressable lights each LED may have a different initial state, so we want to transition them individually.
 * However, to reduce memory usage, we don't want to copy the initial state of each LED. We approximate f(p) by using
 * a repeated application of the lerp function, with an adjusted progress parameter:
 *   f'(p, n) = lerp^n(A, B, a(n))
 * where n is the monotonically increasing iteration number and a(n) is the adjusted progress parameter.
 *
 * Repeated application of the lerp function yields:
 *   lerp^0(A, B, a) = (1-a_0)*A + a_0*B
 *   lerp^1(A, B, a) = (1-a_1)*(1-a_0)*A + (1-a_1)*a_0*B + a_1*B
 *   lerp^2(A, B, a) = (1-a_2)*(1-a_1)*(1-a_0)*A + (1-a_2)*(1-a_1)*a_0*B + (1-a_2)*a_1*B + a_2*B
 *   lerp^n(A, B, a) = [product i=0..n (1-a(i))]*A + [sum i=0..n a(i)*[product j=i+1..n (1-a(i))]]*B
 *
 * To find a(n), equate the addressable transition f'(p) to the linear transition f(p) to get
 *   product i=0..n (1-a(i)) = 1-p(n)
 *   [1-a(n)]*[product i=0..n-1 (1-a(i))] = 1-p(n)
 *   [1-a(n)]*(1-p(n-1)) = 1-p(n)
 *   a(n) = 1-(1-p(n))/(1-p(n-1))
 * where p(n) is the value of p during iteration n.
 *
 * In theory this works perfectly fine, but in practice there is another problem: the state between iterations is stored
 * as an 8-bit integer, losing precision. Due to the iterative nature of the process, this error compounds throughout
 * the transition and becomes very noticable. To avoid this, we store the upper-most 4 bits that would be discarded in
 * the effect data, and add them back in the next iteration. This effectively makes the state 12-bit.
 *
 * Furthermore, all calculations in the inner loop over the LEDs must be done in fixed-with integer math, as there is no
 * FPU available. Proper rounding (instead of the usual flooring with integer math) is necessary, which means that we
 * need a specialized lerp function that calculates c = round((1-p)*a + p*b) with p=0..1.
 *
 * First, convert t into the full range of the type of a and b with maximum value M using r=M*t, and substitute to
 * yield c = round((1-r/M)*a + r/M*b) = round(((M-r)*a + r*b)/M). Set x = (M-r)*a + r*b, and replace division by M
 * with division by N=M+1 (which can be done using a simple bitshift) using x/M = x/N + x/(M*N). This gives us
 * c = round(x/M) = round(x/N + x/(M*N)).
 *
 * We substitute round(u+v) = floor(u) + floor(v) + round(mod(u) + mod(v)) with mod(u) denoting the fractional part of
 * u to yield c = floor(x/N) + floor(x/(M*N)) + round(mod(x/N) + mod(x/(M*N))). As the maximum value of x less then
 * M*N, floor(x/(M*N)) = 0, and we define y = N*mod(x/N) + N*mod(x/(M*N)) to get c = floor(x/N) + round(y/N).
 *
 * Substitution of the definition of the rounding function round(u) = floor(u) + [1 if mod(u)>=1/2] yields
 * c = floor(x/N) + floor(y/N) + [1 if mod(y/N)>=1/2] = floor(x/N) + floor(y/N) + [1 if N*mod(y/N) >= N/2].
 *
 * All remaining terms can be easily calculated:
 * - N*mod(x/N) is the lower half of x.
 * - N*mod(x/(M*N)) is approximated by N*mod(x/N*N), which is the upper half of x.
 * - floor(x/N) is the upper half of x.
 * - floor(y/N) is the upper half of y.
 * - N*mod(y/N) >= N/2 tests if the upper bit in the lower half of y is set.
 */
static inline uint16_t lerp16(uint16_t a, uint16_t b, uint16_t r) {
  uint32_t x = (uint32_t)(65535 - r) * a + (uint32_t) r * b;
  uint32_t y = (x & 0xFFFF) + ((x >> 16) & 0xFFFF);
  return (x >> 16) + (y >> 16) + ((y & 0x8000) >> 15);
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

  // clear effect data
  for (auto led : this->light_)
    led.set_effect_data(0);
}

optional<LightColorValues> AddressableLightTransformer::apply() {
  float smoothed_progress = LightTransitionTransformer::smoothed_progress(this->get_progress_());  // p(n)

  // When running an output-buffer modifying effect, don't try to transition individual LEDs, but instead just fade the
  // LightColorValues. write_state() then picks up the change in brightness, and the color change is picked up by the
  // effects which respect it.
  if (this->light_.is_effect_active())
    return LightColorValues::lerp(this->get_start_values(), this->get_target_values(), smoothed_progress);

  float alpha = 1.0f - (1.0f - smoothed_progress) / (1.0f - this->last_transition_progress_);  // a(n)
  uint16_t alpha16 = roundf(alpha * UINT16_MAX);

  for (auto led : this->light_) {
    uint16_t state = led.get_effect_data();
    uint16_t r = lerp16(((uint16_t) led.get_red() << 8) + ((state & 0x000F) << 4),
                        (uint16_t) this->target_color_.red << 8, alpha16);
    uint16_t g = lerp16(((uint16_t) led.get_green() << 8) + ((state & 0x00F0) << 0),
                        (uint16_t) this->target_color_.green << 8, alpha16);
    uint16_t b = lerp16(((uint16_t) led.get_blue() << 8) + ((state & 0x0F00) >> 4),
                        (uint16_t) this->target_color_.blue << 8, alpha16);
    uint16_t w = lerp16(((uint16_t) led.get_white() << 8) + ((state & 0xF000) >> 8),
                        (uint16_t) this->target_color_.white << 8, alpha16);
    state = ((w & 0x00F0) << 8) | ((b & 0x00F0) << 4) | ((g & 0x00F0) >> 0) | ((b & 0x00F0) >> 4);

    led.set_red(r >> 8);
    led.set_blue(b >> 8);
    led.set_green(g >> 8);
    led.set_white(w >> 8);
    led.set_effect_data(state);
  }

  this->last_transition_progress_ = smoothed_progress;
  this->light_.schedule_show();

  return {};
}

}  // namespace light
}  // namespace esphome
