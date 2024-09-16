#include "tca9548a.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tca9548a {

static const char *const TAG = "tca9548a";

i2c::ErrorCode TCA9548AChannel::readv(uint8_t address, i2c::ReadBuffer *buffers, size_t cnt) {
  auto err = this->parent_->switch_to_channel(channel_);
  if (err != i2c::ERROR_OK)
    return err;
  err = this->parent_->bus_->readv(address, buffers, cnt);
  this->parent_->disable_all_channels();
  return err;
}
i2c::ErrorCode TCA9548AChannel::writev(uint8_t address, i2c::WriteBuffer *buffers, size_t cnt, bool stop) {
  auto err = this->parent_->switch_to_channel(channel_);
  if (err != i2c::ERROR_OK)
    return err;
  err = this->parent_->bus_->writev(address, buffers, cnt, stop);
  this->parent_->disable_all_channels();
  return err;
}

void TCA9548AComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TCA9548A...");
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

i2c::ErrorCode TCA9548AComponent::switch_to_channel(uint8_t channel) {
  if (this->is_failed())
    return i2c::ERROR_NOT_INITIALIZED;

  uint8_t channel_val = 1 << channel;
  return this->write(&channel_val, 1);
}

void TCA9548AComponent::disable_all_channels() {
  if (this->write(&TCA9548A_DISABLE_CHANNELS_COMMAND, 1) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to disable all channels.");
    this->status_set_error();  // couldn't disable channels, set error status
  }
}

}  // namespace tca9548a
}  // namespace esphome
