#include "tca9548a.h"
#include "esphome/core/log.h"

namespace esphome::tca9548a {

ESPHOME_LOG_TAG(TAG, "tca9548a");

i2c::ErrorCode TCA9548AChannel::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                            uint8_t *read_buffer, size_t read_count) {
  // The multiplexer itself is addressed at the bus frequency; only the
  // transaction behind the selected port runs at the port frequency.
  auto err = this->parent_->switch_to_channel(this->channel_);
  if (err != i2c::ERROR_OK)
    return err;

  i2c::I2CBus *bus = this->parent_->bus_;
  if (this->frequency_ == 0) {
    err = bus->write_readv(address, write_buffer, write_count, read_buffer, read_count);
    this->parent_->disable_all_channels();
    return err;
  }

  const uint32_t original_frequency = bus->get_frequency();
  err = bus->set_frequency(this->frequency_);
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to change I²C frequency");
    this->parent_->disable_all_channels();
    return err;
  }

  err = bus->write_readv(address, write_buffer, write_count, read_buffer, read_count);

  if (bus->set_frequency(original_frequency) != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "Failed to restore original I²C frequency");
  }
  this->parent_->disable_all_channels();

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
