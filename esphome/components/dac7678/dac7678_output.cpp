#include "dac7678_output.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/hal.h"

namespace esphome {
namespace dac7678 {

static const char *const TAG = "dac7678";

const uint8_t DAC7678_REG_INPUT_N = 0x00;
const uint8_t DAC7678_REG_SELECT_UPDATE_N = 0x10;
const uint8_t DAC7678_REG_WRITE_N_UPDATE_ALL = 0x20;
const uint8_t DAC7678_REG_WRITE_N_UPDATE_N = 0x30;
const uint8_t DAC7678_REG_POWER = 0x40;
const uint8_t DAC7678_REG_CLEAR_CODE = 0x50;
const uint8_t DAC7678_REG_LDAC = 0x60;
const uint8_t DAC7678_REG_SOFTWARE_RESET = 0x70;
const uint8_t DAC7678_REG_INTERNAL_REF_0 = 0x80;
const uint8_t DAC7678_REG_INTERNAL_REF_1 = 0x90;


void DAC7678Output::setup() {
  ESP_LOGCONFIG(TAG, "Setting up DAC7678OutputComponent...");

  ESP_LOGV(TAG, "Resetting device...");

  // Reset device
  if (!this->write_byte_16(DAC7678_REG_SOFTWARE_RESET, 0x0000)) {
    ESP_LOGE(TAG, "Reset failed");
    this->mark_failed();
    return;
  }
  else
	ESP_LOGV(TAG, "Reset succeeded");

  delayMicroseconds(1000);

  // Set internal reference
  if(this->internal_reference_)
  {
	if (!this->write_byte_16(DAC7678_REG_INTERNAL_REF_0, 1 << 4)) {
	  ESP_LOGE(TAG, "Set internal reference failed");
	  this->mark_failed();
	  return;
	}
	else
	  ESP_LOGV(TAG, "Internal reference enabled");
  }
  
  this->loop();
}

void DAC7678Output::dump_config() {
  if (this->is_failed()) {
    ESP_LOGE(TAG, "Setting up DAC7678 failed!");
  }
  else
	ESP_LOGCONFIG(TAG, "DAC7678 initialised");
  
}

void DAC7678Output::loop() {
  if (this->min_channel_ == 0xFF || !this->update_)
    return;

  for (uint8_t channel = this->min_channel_; channel <= this->max_channel_; channel++) {
    uint16_t input_reg = this->dac_input_reg_[channel];
    ESP_LOGV(TAG, "Channel %01u: input_reg=%04u ", channel, input_reg);

    if (!this->write_byte_16(DAC7678_REG_WRITE_N_UPDATE_N | channel, input_reg << 4)) {
      this->status_set_warning();
      return;
    }
  }

  this->status_clear_warning();
  this->update_ = false;
}

void DAC7678Output::register_channel(DAC7678Channel *channel) {
  auto c = channel->channel_;
  this->min_channel_ = std::min(this->min_channel_, c);
  this->max_channel_ = std::max(this->max_channel_, c);
  channel->set_parent(this);
  ESP_LOGV(TAG, "Registered channel: %01u", channel->channel_);
}

void DAC7678Output::set_channel_value_(uint8_t channel, uint16_t value) {
if (this->dac_input_reg_[channel] != value)
  this->update_ = true;
this->dac_input_reg_[channel] = value;
}

void DAC7678Channel::write_state(float state) {
  const float input_rounded = roundf(state * this->full_scale_);
  auto input = static_cast<uint16_t>(input_rounded);
  this->parent_->set_channel_value_(this->channel_, input);
}

}  // namespace dac7678
}  // namespace esphome
