#include "fastled_light.h"
#include "esphome/core/log.h"

namespace esphome {
namespace fastled_base {

static const char *TAG = "fastled";

void FastLEDLightOutput::setup() {
  ESP_LOGCONFIG(TAG, "Setting up FastLED light...");
  this->controller_->init();
  this->controller_->setLeds(this->leds_, this->num_leds_);
  this->effect_data_ = new uint8_t[this->num_leds_];
  if (!this->max_refresh_rate_.has_value()) {
    this->set_max_refresh_rate(this->controller_->getMaxRefreshRate());
  }
}
void FastLEDLightOutput::dump_config() {
  ESP_LOGCONFIG(TAG, "FastLED light:");
  ESP_LOGCONFIG(TAG, "  Num LEDs: %u", this->num_leds_);
  ESP_LOGCONFIG(TAG, "  Max refresh rate: %u", *this->max_refresh_rate_);
}
void FastLEDLightOutput::loop() {
  if (!this->should_show_())
    return;

  uint32_t now = micros();
  // protect from refreshing too often
  if (*this->max_refresh_rate_ != 0 && (now - this->last_refresh_) < *this->max_refresh_rate_) {
    return;
  }
  this->last_refresh_ = now;
  this->mark_shown_();

  ESP_LOGVV(TAG, "Writing RGB values to bus...");
  this->controller_->showLeds();
}

}  // namespace fastled_base
}  // namespace esphome
