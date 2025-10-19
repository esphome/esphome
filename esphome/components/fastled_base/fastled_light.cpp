#ifdef USE_ARDUINO

#include "fastled_light.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fastled_base {

static const char *const TAG = "fastled";

void FastLEDLightOutput::setup() {
  this->controller_->init();
  this->controller_->setLeds(this->leds_, this->num_leds_);
  this->effect_data_ = new uint8_t[this->num_leds_];  // NOLINT
  if (!this->max_refresh_rate_.has_value()) {
    this->set_max_refresh_rate(this->controller_->getMaxRefreshRate());
  }
}
void FastLEDLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG,
                "FastLED light:\n"
                "  Num LEDs: %u\n"
                "  Max refresh rate: %u",
                this->num_leds_, *this->max_refresh_rate_);
}
void FastLEDLightOutput::write_state(light::LightState *state) {
  // protect from refreshing too often
  uint32_t now = micros();
  if (*this->max_refresh_rate_ != 0 && (now - this->last_refresh_) < *this->max_refresh_rate_) {
    // try again next loop iteration, so that this change won't get lost
    this->schedule_show();
    return;
  }
  this->last_refresh_ = now;
  this->mark_shown_();

  ESP_LOGVV(TAG, "Writing RGB values to bus");
  this->controller_->showLeds(this->state_parent_->current_values.get_brightness() * 255);
}

}  // namespace fastled_base
}  // namespace esphome

#endif  // USE_ARDUINO
