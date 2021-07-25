#include "tlc5947.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tlc5947 {

static const char *const TAG = "tlc5947";

void TLC5947::setup() {
  this->data_pin_->setup();
  this->data_pin_->digital_write(true);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(true);
  this->lat_pin_->setup();
  this->lat_pin_->digital_write(true);
  if (this->outenable_pin_ != nullptr) {
    this->outenable_pin_->setup();
    this->outenable_pin_->digital_write(false);
  }

  this->pwm_amounts_.resize(this->num_chips_ * N_CHANNELS_PER_CHIP, 0);

  ESP_LOGCONFIG(TAG, "Done setting up TLC5947 output component.");
}
void TLC5947::dump_config() {
  ESP_LOGCONFIG(TAG, "TLC5947:");
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
  LOG_PIN("  LAT Pin: ", this->lat_pin_);
  if (this->outenable_pin_ != nullptr)
    LOG_PIN("  OE Pin: ", this->outenable_pin_);
  ESP_LOGCONFIG(TAG, "  Number of chips: %u", this->num_chips_);
}

void TLC5947::loop() {
  if (!this->update_)
    return;

  this->lat_pin_->digital_write(false);

  // push the data out, MSB first, 12 bit word per channel, 24 channels per chip
  for (int32_t ch = N_CHANNELS_PER_CHIP * num_chips_ - 1; ch >= 0; ch--) {
    uint16_t word = pwm_amounts_[ch];
    for (uint8_t bit = 0; bit < 12; bit++) {
      this->clock_pin_->digital_write(false);
      this->data_pin_->digital_write(word & 0x800);
      word <<= 1;

      this->clock_pin_->digital_write(true);
      this->clock_pin_->digital_write(true);  // TWH0>12ns, so we should be fine using this as delay
    }
  }

  this->clock_pin_->digital_write(false);

  // latch the values, so they will be applied
  this->lat_pin_->digital_write(true);
  delayMicroseconds(1);  // TWH1 > 30ns
  this->lat_pin_->digital_write(false);

  this->update_ = false;
}

}  // namespace tlc5947
}  // namespace esphome
