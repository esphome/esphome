#include "power_supply.h"
#include "esphome/core/log.h"

namespace esphome {
namespace power_supply {

static const char *const TAG = "power_supply";

void PowerSupply::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Power Supply...");

  this->pin_->setup();
  this->pin_->digital_write(false);
  this->enabled_ = false;
}
void PowerSupply::dump_config() {
  ESP_LOGCONFIG(TAG, "Power Supply:");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Time to enable: %u ms", this->enable_time_);
  ESP_LOGCONFIG(TAG, "  Keep on time: %.1f s", this->keep_on_time_ / 1000.0f);
}

float PowerSupply::get_setup_priority() const { return setup_priority::IO; }

bool PowerSupply::is_enabled() const { return this->enabled_; }

void PowerSupply::request_high_power() {
  this->cancel_timeout("power-supply-off");
  this->pin_->digital_write(true);

  if (this->active_requests_ == 0) {
    // we need to enable the power supply.
    // cancel old timeout if it exists because we now definitely have a high power mode.
    ESP_LOGD(TAG, "Enabling power supply.");
    delay(this->enable_time_);
  }
  this->enabled_ = true;
  // increase active requests
  this->active_requests_++;
}

void PowerSupply::unrequest_high_power() {
  this->active_requests_--;
  if (this->active_requests_ < 0) {
    // we're just going to use 0 as our new counter.
    this->active_requests_ = 0;
  }

  if (this->active_requests_ == 0) {
    // set timeout for power supply off
    this->set_timeout("power-supply-off", this->keep_on_time_, [this]() {
      ESP_LOGD(TAG, "Disabling power supply.");
      this->pin_->digital_write(false);
      this->enabled_ = false;
    });
  }
}
void PowerSupply::on_shutdown() {
  this->active_requests_ = 0;
  this->pin_->digital_write(false);
}

}  // namespace power_supply
}  // namespace esphome
