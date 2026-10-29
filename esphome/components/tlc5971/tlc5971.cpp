#include "tlc5971.h"
#include "esphome/core/log.h"

namespace esphome::tlc5971 {

ESPHOME_LOG_TAG(TAG, "tlc5971");

// Write command (magic word 0x25), then OUTTMG = 1, EXTGCK = 0, TMGRST = 1, DSPRPT = 1, BLANK = 0 (0x16),
// then 100% global brightness (0x7F) for blue, green and red
static constexpr uint32_t WRITE_COMMAND = (0x25u << 26) | (0x16u << 21) | (0x7Fu << 14) | (0x7Fu << 7) | 0x7Fu;

void TLC5971::setup() {
  this->data_pin_->setup();
  this->data_pin_->digital_write(true);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(true);
}
void TLC5971::dump_config() {
  ESP_LOGCONFIG(TAG,
                "TLC5971:\n"
                "  Number of chips: %u",
                this->num_chips_);
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
}

void TLC5971::loop() {
  this->disable_loop();
  for (uint8_t n = 0; n < this->num_chips_; n++) {
    this->transfer_(WRITE_COMMAND >> 24);
    this->transfer_(WRITE_COMMAND >> 16);
    this->transfer_(WRITE_COMMAND >> 8);
    this->transfer_(WRITE_COMMAND);

    // 16 bits per channel, last channel of the chip first, MSB first
    const uint16_t *chip = this->pwm_amounts_ + n * N_CHANNELS_PER_CHIP;
    for (int8_t c = N_CHANNELS_PER_CHIP - 1; c >= 0; c--) {
      this->transfer_(chip[c] >> 8);
      this->transfer_(chip[c]);
    }
  }
}

void TLC5971::transfer_(uint8_t send) {
  static constexpr uint8_t START_BIT = 0x80;

  bool towrite, lastmosi = !(send & START_BIT);

  for (uint8_t b = START_BIT; b != 0; b = b >> 1) {
    towrite = send & b;
    if (lastmosi != towrite) {
      this->data_pin_->digital_write(towrite);
      lastmosi = towrite;
    }

    this->clock_pin_->digital_write(true);
    this->clock_pin_->digital_write(false);
  }
}
void TLC5971::set_channel_value(uint16_t channel, uint16_t value) {
  if (channel >= this->num_chips_ * N_CHANNELS_PER_CHIP)
    return;
  if (this->pwm_amounts_[channel] != value) {
    this->pwm_amounts_[channel] = value;
    this->enable_loop();
  }
}

}  // namespace esphome::tlc5971
