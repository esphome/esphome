#include "tca9548a.h"
#include "esphome/core/log.h"

namespace esphome {
namespace tca9548a {

static const char *const TAG = "tca9548a";

i2c::ErrorCode TCA9548AChannel::readv(uint8_t address, i2c::ReadBuffer *buffers, size_t cnt) {
  auto err = parent_->switch_to_channel(channel_);
  if (err != i2c::ERROR_OK)
    return err;
  return parent_->bus_->readv(address, buffers, cnt);
}
i2c::ErrorCode TCA9548AChannel::writev(uint8_t address, i2c::WriteBuffer *buffers, size_t cnt) {
  auto err = parent_->switch_to_channel(channel_);
  if (err != i2c::ERROR_OK)
    return err;
  return parent_->bus_->writev(address, buffers, cnt);
}

void TCA9548AComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up TCA9548A...");
  uint8_t status = 0;
  if (this->read(&status, 1) != i2c::ERROR_OK) {
    ESP_LOGI(TAG, "TCA9548A failed");
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
  if (current_channel_ == channel)
    return i2c::ERROR_OK;

  uint8_t channel_val = 1 << channel;
  auto err = this->write_register(0x70, &channel_val, 1);
  if (err == i2c::ERROR_OK) {
    current_channel_ = channel;
  }
  return err;
}

}  // namespace tca9548a
}  // namespace esphome
