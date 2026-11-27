#pragma once

#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::tca9548a {

static const uint8_t TCA9548A_DISABLE_CHANNELS_COMMAND = 0x00;

class TCA9548AComponent;
class TCA9548AChannel final : public i2c::I2CBus {
 public:
  void set_channel(uint8_t channel) { this->channel_ = channel; }
  void set_parent(TCA9548AComponent *parent) { this->parent_ = parent; }

  i2c::ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                             size_t read_count) override;
  i2c::ErrorCode write_prefixed(uint8_t address, const uint8_t *prefix, size_t prefix_len, const uint8_t *data,
                                size_t len) override;

#ifdef I2C_PORT_FREQUENCY_COUNT
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  i2c::ErrorCode switch_frequency(uint32_t frequency) override;
  uint32_t get_frequency() const override;
#endif

 protected:
  /// A transfer to forward upstream; a prefix is written before the data, as write_prefixed() does
  struct Transfer {
    uint8_t address;
    const uint8_t *prefix;
    size_t prefix_len;
    const uint8_t *write_buffer;
    size_t write_count;
    uint8_t *read_buffer;
    size_t read_count;
  };
  /// Select the channel, run the transfer upstream and deselect
  i2c::ErrorCode run_(const Transfer &transfer);
  i2c::ErrorCode forward_(const Transfer &transfer);
#ifdef I2C_PORT_FREQUENCY_COUNT
  uint32_t select_frequency_() const;
  uint32_t transfer_frequency_() const;
  i2c::ErrorCode run_at_port_frequency_(uint32_t select_frequency, const Transfer &transfer);
  i2c::ErrorCode transfer_(uint32_t select_frequency, const Transfer &transfer);
  /// Switch the upstream bus, flagging the multiplexer on failure
  i2c::ErrorCode switch_bus_(uint32_t frequency);
  uint32_t frequency_{0};
#endif
  uint8_t channel_;
  TCA9548AComponent *parent_;
};

class TCA9548AComponent final : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::IO; }
  void update();

  i2c::ErrorCode switch_to_channel(uint8_t channel);
  void disable_all_channels();
#ifdef I2C_PORT_FREQUENCY_COUNT
  /// Addressing frequency, and the default for the ports
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
#endif

 protected:
  friend class TCA9548AChannel;
#ifdef I2C_PORT_FREQUENCY_COUNT
  uint32_t frequency_{0};
#endif
};
}  // namespace esphome::tca9548a
