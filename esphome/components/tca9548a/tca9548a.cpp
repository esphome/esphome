#include "tca9548a.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tca9548a {

static const char *const TAG = "tca9548a";

i2c::ErrorCode TCA9548AChannel::readv(uint8_t address, i2c::ReadBuffer *buffers, size_t cnt) {
  auto err = this->parent_->switch_to_channel(channel_, this->frequency_);
  if (err != i2c::ERROR_OK)
    return err;
  err = this->parent_->bus_->readv(address, buffers, cnt);
  this->parent_->disable_all_channels(this->frequency_ > 0);
  return err;
}
i2c::ErrorCode TCA9548AChannel::writev(uint8_t address, i2c::WriteBuffer *buffers, size_t cnt, bool stop) {
  auto err = this->parent_->switch_to_channel(channel_, this->frequency_);
  if (err != i2c::ERROR_OK)
    return err;
  err = this->parent_->bus_->writev(address, buffers, cnt, stop);
  this->parent_->disable_all_channels(this->frequency_ > 0);
  return err;
}

void TCA9548AComponent::setup() {
  uint8_t status = 0;
  if (this->read(&status, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "TCA9548A failed");
    this->mark_failed();
    return;
  }
  ESP_LOGD(TAG, "Channels currently open: %d", status);
}
void TCA9548AComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "TCA9548A:");
  LOG_I2C_DEVICE(this);
}

i2c::ErrorCode TCA9548AComponent::switch_to_channel(uint8_t channel, uint32_t frequency) {
  if (this->is_failed())
    return i2c::ERROR_NOT_INITIALIZED;

  ESP_LOGD(TAG, "Switching to channel %d", channel);  // DAVe3283: debug only, remove for final
  if (frequency) {
    this->original_frequency_ = this->bus_->get_frequency();
    ESP_LOGD(TAG, "Switching frequency from %" PRIu32 " Hz to %" PRIu32 " Hz", this->original_frequency_,
             frequency);  // DAVe3283: debug only, remove for final
    i2c::ErrorCode err = this->bus_->set_frequency(frequency);
    if (err != i2c::ERROR_OK) {
      this->status_set_error("Failed to change frequency.");
      return err;
    }
  }

  // DAVe3283 ↓ I made this const, does that compile?
  const uint8_t channel_val = 1 << channel;
  return this->write(&channel_val, 1);
}

void TCA9548AComponent::disable_all_channels(bool restore_original_frequency) {
  ESP_LOGD(TAG, "Disabling all channels");  // DAVe3283: debug only, remove for final
  if (this->write(&TCA9548A_DISABLE_CHANNELS_COMMAND, 1) != i2c::ERROR_OK) {
    this->status_set_error("Failed to disable all channels.");
  }
  if (restore_original_frequency) {
    if (this->bus_->set_frequency(this->original_frequency_) != i2c::ERROR_OK) {
      this->status_set_error("Failed to restore original frequency.");
    }
  }
}

}  // namespace tca9548a
}  // namespace esphome
