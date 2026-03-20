#include "sm16716.h"
#include "esphome/core/log.h"

namespace esphome {
namespace sm16716 {

static const char *const TAG = "sm16716";

void SM16716::setup() {
  this->data_pin_->setup();
  this->data_pin_->digital_write(false);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(false);
  this->pwm_amounts_.resize(this->num_channels_, 0);
}
void SM16716::dump_config() {
  ESP_LOGCONFIG(TAG,
                "SM16716:\n"
                "  Total number of channels: %u\n"
                "  Number of chips: %u",
                this->num_channels_, this->num_chips_);
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
}
void SM16716::loop() {
  if (!this->update_)
    return;

  for (uint8_t i = 0; i < 50; i++) {
    this->write_bit_(false);
  }

  // send 25 bits (1 start bit plus 24 data bits) for each chip
  for (uint8_t index = 0; index < this->num_channels_; index++) {
    // send a start bit initially and after every 3 channels
    if (index % 3 == 0) {
      this->write_bit_(true);
    }

    this->write_byte_(this->pwm_amounts_[index]);
  }

  // send a blank 25 bits to signal the end
  this->write_bit_(false);
  this->write_byte_(0);
  this->write_byte_(0);
  this->write_byte_(0);

  this->update_ = false;
}

}  // namespace sm16716
}  // namespace esphome
