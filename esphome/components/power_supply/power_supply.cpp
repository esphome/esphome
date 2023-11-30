#include "power_supply.h"
#include "esphome/core/log.h"

namespace esphome {
namespace power_supply {

static const char *const TAG = "power_supply";

void PowerSupply::setup() {
  ESP_LOGCONFIG(TAG, "Setting up Power Supply...");

  this->pin_->setup();
  this->pin_->digital_write(false);
  if (this->enable_on_boot_)
    this->request_high_power();
}
void PowerSupply::dump_config() {
  ESP_LOGCONFIG(TAG, "Power Supply:");
  LOG_PIN("  Pin: ", this->pin_);
  ESP_LOGCONFIG(TAG, "  Time to enable: %" PRIu32 " ms", this->enable_time_);
  ESP_LOGCONFIG(TAG, "  Keep on time: %.1f s", this->keep_on_time_ / 1000.0f);
  if (this->enable_on_boot_)
    ESP_LOGCONFIG(TAG, "  Enabled at startup: True");
}

float PowerSupply::get_setup_priority() const { return setup_priority::IO; }

bool PowerSupply::is_enabled() const { return this->active_requests_ != 0; }

void PowerSupply::request_high_power() {
  if (this->active_requests_ == 0) {
    this->cancel_timeout("power-supply-off");
    ESP_LOGD(TAG, "Enabling power supply.");
    this->pin_->digital_write(true);
    delay(this->enable_time_);
  }
  this->active_requests_++;
}

void PowerSupply::unrequest_high_power() {
  if (this->active_requests_ == 0) {
    ESP_LOGW(TAG, "Invalid call to unrequest_high_power");
    return;
  }
  this->active_requests_--;
  if (this->active_requests_ == 0) {
    this->set_timeout("power-supply-off", this->keep_on_time_, [this]() {
      ESP_LOGD(TAG, "Disabling power supply.");
      this->pin_->digital_write(false);
    });
  }
}
void PowerSupply::on_shutdown() {
  this->active_requests_ = 0;
  this->pin_->digital_write(false);
}

}  // namespace power_supply
}  // namespace esphome
