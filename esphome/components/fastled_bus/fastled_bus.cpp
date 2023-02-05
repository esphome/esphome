#include "fastled_bus.h"

namespace esphome {
namespace fastled_bus {

void FastLEDBus::setup() {
  this->dump_config();
  for (int i = 0; i < this->num_chips_ * this->chip_channels_; i++) {
    this->channels_[i] = 0;
  }
  this->controller_->init();
  this->controller_->setLeds((CRGB *) this->channels_, this->num_chips_);
  if (!this->max_refresh_rate_.has_value()) {
    this->set_max_refresh_rate(this->controller_->getMaxRefreshRate());
  }
  this->schedule_refresh();
}

void FastLEDBus::dump_config() {
  ESP_LOGCONFIG("fastled:bus:", "FastLED bus:%d:%p", this->num_chips_, this->controller_);
}

void FastLEDBus::loop() {
  if (!this->do_refresh_) {
    return;
  }
  // protect from refreshing too often
  uint32_t now = micros();
  if (*this->max_refresh_rate_ != 0 && (now - this->last_refresh_) < *this->max_refresh_rate_) {
    return;
  }
  this->last_refresh_ = now;
  this->controller_->showLeds();
  this->do_refresh_ = false;
}

}  // namespace fastled_bus
}  // namespace esphome
