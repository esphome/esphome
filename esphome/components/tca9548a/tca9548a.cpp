#include "tca9548a.h"
#include "esphome/core/log.h"

namespace esphome::tca9548a {

ESPHOME_LOG_TAG(TAG, "tca9548a");

i2c::ErrorCode TCA9548AChannel::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                            uint8_t *read_buffer, size_t read_count) {
  i2c::I2CBus *bus = this->parent_->bus_;
#ifdef I2C_PORT_FREQUENCY_COUNT
  // The multiplexer's own frequency covers its select and deselect and is the
  // default for its ports; a port frequency applies to the transfer behind it.
  // Without a multiplexer frequency the port frequency covers the select too,
  // since a multiplexer tolerates a faster bus but may not answer at that speed.
  const uint32_t mux_frequency = this->parent_->frequency_;
  const uint32_t port_frequency = this->frequency_ != 0 ? this->frequency_ : mux_frequency;
  const uint32_t select_frequency = mux_frequency != 0 ? mux_frequency : port_frequency;
  if (select_frequency != 0) {
    const uint32_t original_frequency = bus->get_frequency();
    auto err = this->switch_bus_(bus, select_frequency);
    if (err == i2c::ERROR_OK) {
      err = this->parent_->switch_to_channel(this->channel_);
      if (err == i2c::ERROR_OK) {
        if (port_frequency == select_frequency || (err = this->switch_bus_(bus, port_frequency)) == i2c::ERROR_OK) {
          err = bus->write_readv(address, write_buffer, write_count, read_buffer, read_count);
        }
        if (port_frequency != select_frequency) {
          this->switch_bus_(bus, select_frequency);
        }
        this->parent_->disable_all_channels();
      }
    }
    this->switch_bus_(bus, original_frequency);
    return err;
  }
#endif
  auto err = this->parent_->switch_to_channel(this->channel_);
  if (err != i2c::ERROR_OK)
    return err;
  err = bus->write_readv(address, write_buffer, write_count, read_buffer, read_count);
  this->parent_->disable_all_channels();
  return err;
}

#ifdef I2C_PORT_FREQUENCY_COUNT
// Both act on the shared upstream bus: a multiplexer behind this port
// switches it, and restores whatever it ran at before. When ports at both
// levels set a frequency, the outer port's is the one the transfer uses,
// and the inner port's covers the outer multiplexer's select.
i2c::ErrorCode TCA9548AChannel::switch_frequency(uint32_t frequency) {
  return this->parent_->bus_->switch_frequency(frequency);
}

i2c::ErrorCode TCA9548AChannel::switch_bus_(i2c::I2CBus *bus, uint32_t frequency) {
  const i2c::ErrorCode err = bus->switch_frequency(frequency);
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
  if (this->frequency_ != 0) {
    this->bus_->switch_frequency(original_frequency);
  }
#endif
  if (err != i2c::ERROR_OK) {
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
