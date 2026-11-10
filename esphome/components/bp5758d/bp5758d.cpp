#include "bp5758d.h"
#include "esphome/core/log.h"

namespace esphome::bp5758d {

ESPHOME_LOG_TAG(TAG, "bp5758d");

static constexpr uint8_t BP5758D_MODEL_ID = 0b10000000;
static constexpr uint8_t BP5758D_ADDR_STANDBY = 0b00000000;
// Note, channel start address seems ambiguous and mis-translated.
// Documentation states all are "invalid sleep"
// All 3 values appear to activate all 5 channels.  Using the mapping
// from BP1658CJ ordering since it won't break anything.
static constexpr uint8_t BP5758D_ADDR_START_3CH = 0b00010000;
static constexpr uint8_t BP5758D_ADDR_START_2CH = 0b00100000;
static constexpr uint8_t BP5758D_ADDR_START_5CH = 0b00110000;
static constexpr uint8_t BP5758D_ALL_DATA_CHANNEL_ENABLEMENT = 0b00011111;

static constexpr uint8_t BP5758D_DELAY = 2;

void BP5758D::setup() {
  this->data_pin_->setup();
  this->data_pin_->digital_write(false);
  delayMicroseconds(BP5758D_DELAY);
  this->clock_pin_->setup();
  this->clock_pin_->digital_write(false);
  delayMicroseconds(BP5758D_DELAY);
}
void BP5758D::dump_config() {
  ESP_LOGCONFIG(TAG, "BP5758D:");
  LOG_PIN("  Data Pin: ", this->data_pin_);
  LOG_PIN("  Clock Pin: ", this->clock_pin_);
}

void BP5758D::loop() {
  this->disable_loop();

  const bool color_on = this->pwm_amounts_[0] > 0 || this->pwm_amounts_[1] > 0 || this->pwm_amounts_[2] > 0;
  const bool white_on = this->pwm_amounts_[3] > 0 || this->pwm_amounts_[4] > 0;
  uint8_t data[17]{};
  if (!color_on && !white_on) {
    // First turn all channels off
    data[0] = BP5758D_MODEL_ID + BP5758D_ADDR_START_5CH;
    this->write_buffer_(data, sizeof(data));
    // Then sleep
    data[0] = BP5758D_MODEL_ID + BP5758D_ADDR_STANDBY;
    this->write_buffer_(data, sizeof(data));
    return;
  }

  uint8_t addr = BP5758D_ADDR_START_5CH;
  if (!color_on) {
    addr = BP5758D_ADDR_START_2CH;
  } else if (!white_on) {
    addr = BP5758D_ADDR_START_3CH;
  }
  data[0] = BP5758D_MODEL_ID + addr;
  data[1] = BP5758D_ALL_DATA_CHANNEL_ENABLEMENT;
  for (size_t i = 0; i < this->pwm_amounts_.size(); i++) {
    // A channel that is off gets no current
    data[2 + i] = this->pwm_amounts_[i] > 0 ? this->channel_current_[i] : 0;
    data[7 + i * 2] = this->pwm_amounts_[i] & 0x1F;
    data[8 + i * 2] = (this->pwm_amounts_[i] >> 5) & 0x1F;
  }
  this->write_buffer_(data, sizeof(data));
}

void BP5758D::write_channel(uint8_t channel, float state) {
  auto value = static_cast<uint16_t>(state * 0x3FF);
  if (this->pwm_amounts_[channel] != value) {
    this->pwm_amounts_[channel] = value;
    this->enable_loop();
  }
}

void BP5758D::write_bit_(bool value) {
  this->data_pin_->digital_write(value);
  delayMicroseconds(BP5758D_DELAY);
  this->clock_pin_->digital_write(true);
  delayMicroseconds(BP5758D_DELAY);
  this->clock_pin_->digital_write(false);
  delayMicroseconds(BP5758D_DELAY);
}

void BP5758D::write_byte_(uint8_t data) {
  for (uint8_t mask = 0x80; mask; mask >>= 1) {
    this->write_bit_(data & mask);
  }

  // ack bit
  this->data_pin_->pin_mode(gpio::FLAG_INPUT);
  this->clock_pin_->digital_write(true);
  delayMicroseconds(BP5758D_DELAY);
  this->clock_pin_->digital_write(false);
  delayMicroseconds(BP5758D_DELAY);
  this->data_pin_->pin_mode(gpio::FLAG_OUTPUT);
}

void BP5758D::write_buffer_(const uint8_t *buffer, uint8_t size) {
  this->data_pin_->digital_write(false);
  delayMicroseconds(BP5758D_DELAY);
  this->clock_pin_->digital_write(false);
  delayMicroseconds(BP5758D_DELAY);

  for (uint32_t i = 0; i < size; i++) {
    this->write_byte_(buffer[i]);
  }

  this->clock_pin_->digital_write(true);
  delayMicroseconds(BP5758D_DELAY);
  this->data_pin_->digital_write(true);
  delayMicroseconds(BP5758D_DELAY);
}

}  // namespace esphome::bp5758d
