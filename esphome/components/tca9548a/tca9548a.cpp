#include "tca9548a.h"
#include "esphome/core/log.h"

namespace esphome::tca9548a {

static const char *const TAG = "tca9548a";

i2c::ErrorCode TCA9548AChannel::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                            uint8_t *read_buffer, size_t read_count) {
  const uint32_t original_frequency = this->parent_->bus_->get_frequency();
  if (this->frequency_) {
    const i2c::ErrorCode err = this->parent_->bus_->set_frequency(this->frequency_);
    if (err != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Failed to change I²C frequency");
      return err;
    }
  }

  auto err = this->parent_->switch_to_channel(this->channel_);
  if (err != i2c::ERROR_OK)
    return err;
  err = this->parent_->bus_->write_readv(address, write_buffer, write_count, read_buffer, read_count);
  this->parent_->disable_all_channels();

  if (this->frequency_) {
    if (this->parent_->bus_->set_frequency(original_frequency) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "Failed to restore original I²C frequency");
    }
  }

  return err;
}

i2c::ErrorCode TCA9548AChannel::set_frequency(uint32_t frequency) {
  this->frequency_ = frequency;
  return i2c::ERROR_OK;
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

i2c::ErrorCode TCA9548AComponent::switch_to_channel(uint8_t channel) {
  if (this->is_failed())
    return i2c::ERROR_NOT_INITIALIZED;

  const uint8_t channel_val = 1 << channel;
  return this->write(&channel_val, 1);
}

void TCA9548AComponent::disable_all_channels() {
  if (this->write(&TCA9548A_DISABLE_CHANNELS_COMMAND, 1) != i2c::ERROR_OK) {
    this->status_set_error(LOG_STR("Failed to disable all channels."));
  }
}

}  // namespace esphome::tca9548a
