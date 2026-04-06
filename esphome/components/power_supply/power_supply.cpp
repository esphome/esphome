#include "power_supply.h"
#include "esphome/core/log.h"

namespace esphome {
namespace power_supply {

static const char *const TAG = "power_supply";

void PowerSupply::setup() {
  this->pin_->setup();
  this->pin_->digital_write(false);
  if (this->enable_on_boot_)
    this->request_high_power();
}
void PowerSupply::dump_config() {
  ESP_LOGCONFIG(TAG,
                "Power Supply:\n"
                "  Time to enable: %" PRIu32 " ms\n"
                "  Keep on time: %" PRIu32 " s\n"
                "  Enable at startup: %s",
                this->enable_time_, this->keep_on_time_ / 1000u, YESNO(this->enable_on_boot_));
  LOG_PIN("  Pin: ", this->pin_);
}

float PowerSupply::get_setup_priority() const { return setup_priority::IO; }

bool PowerSupply::is_enabled() const { return this->active_requests_ != 0; }

void PowerSupply::request_high_power() {
  if (this->active_requests_ == 0) {
    this->cancel_timeout("power-supply-off");
    ESP_LOGV(TAG, "Enabling");
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
      ESP_LOGV(TAG, "Disabling");
      this->pin_->digital_write(false);
    });
  }
}
void PowerSupply::on_powerdown() {
  this->active_requests_ = 0;
  this->pin_->digital_write(false);
}

}  // namespace power_supply
}  // namespace esphome
