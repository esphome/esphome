#include "tca9548a.h"

#include <algorithm>
#include "esphome/core/log.h"

namespace esphome::tca9548a {

ESPHOME_LOG_TAG(TAG, "tca9548a");

i2c::ErrorCode TCA9548AChannel::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                            uint8_t *read_buffer, size_t read_count) {
#ifdef I2C_PORT_FREQUENCY_COUNT
  const uint32_t select_frequency = this->select_frequency_();
  if (select_frequency != 0) {
    return this->write_readv_at_port_frequency_(select_frequency, address, write_buffer, write_count, read_buffer,
                                                read_count);
  }
#endif
  auto err = this->parent_->switch_to_channel(this->channel_);
  if (err != i2c::ERROR_OK)
    return err;
  err = this->parent_->bus_->write_readv(address, write_buffer, write_count, read_buffer, read_count);
  this->parent_->disable_all_channels();
  return err;
}

#ifdef I2C_PORT_FREQUENCY_COUNT
// The multiplexer's own frequency, else the slower of the bus and the port
uint32_t TCA9548AChannel::select_frequency_() const {
  if (this->parent_->frequency_ != 0)
    return this->parent_->frequency_;
  if (this->frequency_ == 0)
    return 0;
  // A bus that does not report its frequency gets the port's, and fails the switch
  const uint32_t bus_frequency = this->parent_->bus_->get_frequency();
  return bus_frequency != 0 ? std::min(this->frequency_, bus_frequency) : this->frequency_;
}

// The port frequency, else the multiplexer's
uint32_t TCA9548AChannel::transfer_frequency_() const {
  return this->frequency_ != 0 ? this->frequency_ : this->parent_->frequency_;
}

i2c::ErrorCode TCA9548AChannel::write_readv_at_port_frequency_(uint32_t select_frequency, uint8_t address,
                                                               const uint8_t *write_buffer, size_t write_count,
                                                               uint8_t *read_buffer, size_t read_count) {
  const uint32_t original_frequency = this->parent_->bus_->get_frequency();
  // A failed switch leaves the bus as it was
  auto err = this->switch_bus_(select_frequency);
  if (err != i2c::ERROR_OK)
    return err;

  err = this->parent_->switch_to_channel(this->channel_);
  if (err == i2c::ERROR_OK) {
    err = this->transfer_(select_frequency, address, write_buffer, write_count, read_buffer, read_count);
    this->parent_->disable_all_channels();
  }
  const i2c::ErrorCode restored = this->switch_bus_(original_frequency);
  return err != i2c::ERROR_OK ? err : restored;
}

// Transfer at the port frequency, back to the select frequency for the deselect
i2c::ErrorCode TCA9548AChannel::transfer_(uint32_t select_frequency, uint8_t address, const uint8_t *write_buffer,
                                          size_t write_count, uint8_t *read_buffer, size_t read_count) {
  const uint32_t transfer_frequency = this->transfer_frequency_();
  if (transfer_frequency == select_frequency) {
    return this->parent_->bus_->write_readv(address, write_buffer, write_count, read_buffer, read_count);
  }
  auto err = this->switch_bus_(transfer_frequency);
  if (err == i2c::ERROR_OK) {
    err = this->parent_->bus_->write_readv(address, write_buffer, write_count, read_buffer, read_count);
  }
  const i2c::ErrorCode restored = this->switch_bus_(select_frequency);
  return err != i2c::ERROR_OK ? err : restored;
}

// Forward to the shared upstream bus
i2c::ErrorCode TCA9548AChannel::switch_frequency(uint32_t frequency) {
  return this->parent_->bus_->switch_frequency(frequency);
}

i2c::ErrorCode TCA9548AChannel::switch_bus_(uint32_t frequency) {
  const i2c::ErrorCode err = this->switch_frequency(frequency);
  if (err != i2c::ERROR_OK) {
    this->parent_->status_set_error(LOG_STR("Failed to switch the bus frequency"));
  }
  return err;
}

uint32_t TCA9548AChannel::get_frequency() const { return this->parent_->bus_->get_frequency(); }
#endif

void TCA9548AComponent::setup() {
  uint8_t status = 0;
#ifdef I2C_PORT_FREQUENCY_COUNT
  const uint32_t original_frequency = this->bus_->get_frequency();
  if (this->frequency_ != 0 && this->bus_->switch_frequency(this->frequency_) != i2c::ERROR_OK) {
    this->mark_failed(LOG_STR("Failed to switch the bus frequency"));
    return;
  }
#endif
  const i2c::ErrorCode err = this->read(&status, 1);
#ifdef I2C_PORT_FREQUENCY_COUNT
  const bool restored = this->frequency_ == 0 || this->bus_->switch_frequency(original_frequency) == i2c::ERROR_OK;
#endif
  if (err != i2c::ERROR_OK) {
    ESP_LOGE(TAG, "TCA9548A failed");
    this->mark_failed();
    return;
  }
#ifdef I2C_PORT_FREQUENCY_COUNT
  if (!restored) {
    this->mark_failed(LOG_STR("Failed to switch the bus frequency"));
    return;
  }
#endif
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
