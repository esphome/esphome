#pragma once

#ifdef USE_LIBRETINY

#include "esphome/core/component.h"
#include "esphome/core/gpio.h"
#include "i2c_bus.h"

namespace esphome::i2c {

/// Bit-banged I2C controller for LibreTiny chips without an Arduino Wire implementation (BK72xx).
/// SDA and SCL can be any two GPIO pins; both lines are driven open-drain style (pulled low or released),
/// so the bus needs pull-ups (the internal pull-up is enabled while released).
class SoftwareI2CBus final : public I2CBus, public Component {
 public:
  void setup() override;
  void dump_config() override;
  ErrorCode write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                        size_t read_count) override;
  float get_setup_priority() const override { return setup_priority::BUS; }

  void set_scan(bool scan) { this->scan_ = scan; }
  void set_sda_pin(InternalGPIOPin *sda_pin) { this->sda_pin_ = sda_pin; }
  void set_scl_pin(InternalGPIOPin *scl_pin) { this->scl_pin_ = scl_pin; }
  void set_frequency(uint32_t frequency) { this->frequency_ = frequency; }
  /// Maximum time in microseconds a device may hold SCL low (clock stretching).
  void set_timeout(uint32_t timeout) { this->timeout_ = timeout; }

 protected:
  bool recover_();
  void start_();
  void stop_();
  bool release_scl_();
  bool write_byte_(uint8_t data);
  uint8_t read_byte_(bool ack);

  InternalGPIOPin *sda_pin_{nullptr};
  InternalGPIOPin *scl_pin_{nullptr};
  uint32_t frequency_;
  uint32_t timeout_{1000};
  uint32_t half_period_us_{5};
  bool scl_timed_out_{false};
  bool recovered_{false};
  bool initialized_{false};
};

}  // namespace esphome::i2c

#endif  // USE_LIBRETINY
