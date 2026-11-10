#include "bp1658cj.h"
#include "esphome/core/log.h"

namespace esphome::bp1658cj {

ESPHOME_LOG_TAG(TAG, "bp1658cj");

static constexpr uint8_t BP1658CJ_MODEL_ID = 0x80;
static constexpr uint8_t BP1658CJ_ADDR_STANDBY = 0x0;
static constexpr uint8_t BP1658CJ_ADDR_START_3CH = 0x10;
static constexpr uint8_t BP1658CJ_ADDR_START_2CH = 0x20;
static constexpr uint8_t BP1658CJ_ADDR_START_5CH = 0x30;

static constexpr uint8_t BP1658CJ_DELAY = 2;

void BP1658CJ::setup() {
  this->data_pin_->setup();
  this->data_pin_->digital_write(false);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(false);
}
void BP1658CJ::dump_config() {
  ESP_LOGCONFIG(TAG,
                "BP1658CJ:\n"
                "  Color Channels Max Power: %u\n"
                "  White Channels Max Power: %u",
                this->max_power_color_channels_, this->max_power_white_channels_);
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
}

void BP1658CJ::loop() {
  this->disable_loop();

  const bool color_on = this->pwm_amounts_[0] > 0 || this->pwm_amounts_[1] > 0 || this->pwm_amounts_[2] > 0;
  const bool white_on = this->pwm_amounts_[3] > 0 || this->pwm_amounts_[4] > 0;
  uint8_t data[12]{};
  uint8_t addr = BP1658CJ_ADDR_START_5CH;
  if (!color_on && white_on) {
    addr = BP1658CJ_ADDR_START_2CH;
  } else if (color_on && !white_on) {
    addr = BP1658CJ_ADDR_START_3CH;
  }
  data[0] = BP1658CJ_MODEL_ID + addr;
  // Only the max power of the channel groups being driven is sent
  data[1] = (color_on ? this->max_power_color_channels_ << 4 : 0) | (white_on ? this->max_power_white_channels_ : 0);
  for (size_t i = 0; i < this->pwm_amounts_.size(); i++) {
    data[2 + i * 2] = this->pwm_amounts_[i] & 0x1F;
    data[3 + i * 2] = (this->pwm_amounts_[i] >> 5) & 0x1F;
  }
  this->write_buffer_(data, sizeof(data));

  if (!color_on && !white_on) {
    // All channels are now off, so put the chip to sleep
    data[0] = BP1658CJ_MODEL_ID + BP1658CJ_ADDR_STANDBY;
    this->write_buffer_(data, sizeof(data));
  }
}

void BP1658CJ::write_channel(uint8_t channel, float state) {
  auto value = static_cast<uint16_t>(state * 0x3FF);
  if (this->pwm_amounts_[channel] != value) {
    this->pwm_amounts_[channel] = value;
    this->enable_loop();
  }
}

void BP1658CJ::write_bit_(bool value) {
  this->data_pin_->digital_write(value);
  delayMicroseconds(BP1658CJ_DELAY);
  this->clock_pin_->digital_write(true);
  delayMicroseconds(BP1658CJ_DELAY);
  this->clock_pin_->digital_write(false);
  delayMicroseconds(BP1658CJ_DELAY);
}

void BP1658CJ::write_byte_(uint8_t data) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    this->write_bit_(data & mask);
  }

  // ack bit
  this->data_pin_->pin_mode(gpio::FLAG_INPUT);
  this->clock_pin_->digital_write(true);
  delayMicroseconds(BP1658CJ_DELAY);
  this->clock_pin_->digital_write(false);
  delayMicroseconds(BP1658CJ_DELAY);
  this->data_pin_->pin_mode(gpio::FLAG_OUTPUT);
}

void BP1658CJ::write_buffer_(const uint8_t *buffer, uint8_t size) {
  this->data_pin_->digital_write(false);
  delayMicroseconds(BP1658CJ_DELAY);
  this->clock_pin_->digital_write(false);
  delayMicroseconds(BP1658CJ_DELAY);

  for (uint32_t i = 0; i < size; i++) {
    this->write_byte_(buffer[i]);
  }

  this->clock_pin_->digital_write(true);
  delayMicroseconds(BP1658CJ_DELAY);
  this->data_pin_->digital_write(true);
  delayMicroseconds(BP1658CJ_DELAY);
}

}  // namespace esphome::bp1658cj
