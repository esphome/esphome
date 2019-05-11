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

#ifdef USE_POWER_SUPPLY
  if (this->power_supply_ != nullptr) {
    bool is_on = false;
    if (this->power_supply_keep_on_ && this->effect_active_ && this->has_requested_high_power_) {
      is_on = true;
    } else {
      for (int i = 0; i < this->num_leds_; i++) {
        if (bool(this->leds_[i])) {
          is_on = true;
          break;
        }
      }
    }

    if (is_on && !this->has_requested_high_power_) {
      this->power_supply_->request_high_power();
      this->has_requested_high_power_ = true;
    }
    if (!is_on && this->has_requested_high_power_) {
      this->power_supply_->unrequest_high_power();
      this->has_requested_high_power_ = false;
    }
  }
#endif
  this->controller_->showLeds();
}

}  // namespace fastled_base
}  // namespace esphome
