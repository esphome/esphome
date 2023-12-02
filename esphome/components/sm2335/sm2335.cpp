#include "sm2335.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sm2335 {

static const char *const TAG = "sm2335";

void SM2335::setup() {
  ESP_LOGCONFIG(TAG, "Setting up sm2335 Output Component...");
  this->data_pin_->setup();
  this->data_pin_->digital_write(true);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(true);
  this->pwm_amounts_.resize(5, 0);
}

void SM2335::dump_config() {
  ESP_LOGCONFIG(TAG, "sm2335:");
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
  ESP_LOGCONFIG(TAG, "  Color Channels Max Power: %u", this->max_power_color_channels_);
  ESP_LOGCONFIG(TAG, "  White Channels Max Power: %u", this->max_power_white_channels_);
}

}  // namespace sm2335
}  // namespace esphome
