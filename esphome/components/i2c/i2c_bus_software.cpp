#ifdef USE_LIBRETINY

#include "i2c_bus_software.h"
#include <cinttypes>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::i2c {

ESPHOME_LOG_TAG(TAG, "i2c.software");

// Maximum bytes to log in hex format (truncates larger transfers)
static constexpr size_t I2C_MAX_LOG_BYTES = 32;
// Clock pulses used to free a device that is holding SDA low
static constexpr uint8_t RECOVERY_CLOCK_PULSES = 9;

// A line is released by switching the pin to input with pull-up and pulled low by driving it as output.
static inline void release_line(InternalGPIOPin *pin) { pin->pin_mode(gpio::FLAG_INPUT | gpio::FLAG_PULLUP); }
static inline void pull_line_low(InternalGPIOPin *pin) {
  // Switch to output first: on LibreTiny digital_write() on an input only changes the pull resistor
  // and does not set the output level. The latch is only ever written low, so this cannot glitch the line high.
  pin->pin_mode(gpio::FLAG_OUTPUT);
  pin->digital_write(false);
}

void SoftwareI2CBus::setup() {
  this->update_half_period_();
  this->sda_pin_->setup();
  this->scl_pin_->setup();
  release_line(this->sda_pin_);
  release_line(this->scl_pin_);
  if (!this->recover_()) {
    // A line that stays low would be read as an ACK for every address and as zero data
    ESP_LOGE(TAG, "SDA or SCL is held low, the bus cannot be used");
    this->mark_failed();
    return;
  }
  this->initialized_ = true;
  if (this->scan_) {
    ESP_LOGV(TAG, "Scanning bus for active devices");
    this->i2c_scan_();
  }
}

void SoftwareI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus (software):");
  LOG_PIN("  SDA Pin: ", this->sda_pin_);
  LOG_PIN("  SCL Pin: ", this->scl_pin_);
  ESP_LOGCONFIG(TAG,
                "  Frequency: %" PRIu32 " Hz\n"
                "  Clock stretch timeout: %" PRIu32 " us",
                this->frequency_, this->timeout_);
  if (this->recovered_) {
    ESP_LOGCONFIG(TAG, "  Recovery: bus successfully recovered");
  }
  if (this->scan_) {
    ESP_LOGI(TAG, "Results from bus scan:");
    if (this->scan_results_.empty()) {
      ESP_LOGI(TAG, "Found no devices");
    } else {
      for (const auto &s : this->scan_results_) {
        if (s.second) {
          ESP_LOGI(TAG, "Found device at address 0x%02X", s.first);
        } else {
          ESP_LOGE(TAG, "Unknown error at address 0x%02X", s.first);
        }
      }
    }
  }
}

// Half of one clock period in microseconds, rounded up. The frequency is validated to be non-zero.
void SoftwareI2CBus::update_half_period_() {
  this->half_period_us_ = (500000 + this->frequency_ - 1) / this->frequency_;
}

#ifdef I2C_PORT_FREQUENCY_COUNT
ErrorCode SoftwareI2CBus::switch_frequency(uint32_t frequency) {
  if (frequency == 0)
    return ERROR_INVALID_ARGUMENT;
  this->frequency_ = frequency;
  this->update_half_period_();
  return ERROR_OK;
}
#endif

bool SoftwareI2CBus::recover_() {
  this->timed_out_ = false;
  // A device may still be stretching the clock, give SCL the configured time to rise before looking at SDA
  release_line(this->scl_pin_);
  this->release_scl_();
  // A device that was interrupted in the middle of a transfer can hold SDA low; clock it out.
  for (uint8_t i = 0; i < RECOVERY_CLOCK_PULSES && !this->sda_pin_->digital_read(); i++) {
    pull_line_low(this->scl_pin_);
    delayMicroseconds(this->half_period_us_);
    // wait for the rising edge, the device may stretch the clock
    if (!this->release_scl_())
      break;
    delayMicroseconds(this->half_period_us_);
    this->recovered_ = true;
  }
  bool ok = this->sda_pin_->digital_read() && this->scl_pin_->digital_read();
  if (ok && this->recovered_) {
    // end the interrupted transfer with a STOP so every device returns to idle
    this->timed_out_ = false;
    pull_line_low(this->sda_pin_);
    delayMicroseconds(this->half_period_us_);
    release_line(this->scl_pin_);
    delayMicroseconds(this->half_period_us_);
    release_line(this->sda_pin_);
  }
  if (!ok) {
    ESP_LOGW(TAG, "Bus recovery failed, a line is held low");
    this->recovered_ = false;
  }
  this->timed_out_ = false;
  return ok;
}

// Release SCL and wait for it to go high, a device may stretch the clock.
bool SoftwareI2CBus::release_scl_() {
  release_line(this->scl_pin_);
  uint32_t start = micros();
  while (!this->scl_pin_->digital_read()) {
    if (micros() - start > this->timeout_) {
      this->timed_out_ = true;
      return false;
    }
  }
  return true;
}

void SoftwareI2CBus::start_() {
  // Also generates a repeated start when SCL is low and SDA is released
  release_line(this->sda_pin_);
  // After a byte the device may still hold SDA low for its ACK. Without SDA being high before SCL rises there is
  // no falling edge, so the (repeated) start would not be seen. Wait until the device lets go.
  uint32_t start = micros();
  while (!this->sda_pin_->digital_read()) {
    if (micros() - start > this->timeout_) {
      this->timed_out_ = true;
      return;
    }
  }
  delayMicroseconds(this->half_period_us_);
  this->release_scl_();
  delayMicroseconds(this->half_period_us_);
  pull_line_low(this->sda_pin_);
  delayMicroseconds(this->half_period_us_);
  pull_line_low(this->scl_pin_);
}

void SoftwareI2CBus::stop_() {
  if (this->timed_out_) {
    // SCL is stuck low, a STOP cannot be generated; just release both lines
    release_line(this->scl_pin_);
    release_line(this->sda_pin_);
    return;
  }
  pull_line_low(this->sda_pin_);
  delayMicroseconds(this->half_period_us_);
  this->release_scl_();
  delayMicroseconds(this->half_period_us_);
  release_line(this->sda_pin_);
  delayMicroseconds(this->half_period_us_);
}

/// Shift out one byte, returns true if the device acknowledged it.
bool SoftwareI2CBus::write_byte_(uint8_t data) {
  for (uint8_t mask = 0x80; mask != 0 && !this->timed_out_; mask >>= 1) {
    if (data & mask) {
      release_line(this->sda_pin_);
    } else {
      pull_line_low(this->sda_pin_);
    }
    delayMicroseconds(this->half_period_us_);
    this->release_scl_();
    delayMicroseconds(this->half_period_us_);
    pull_line_low(this->scl_pin_);
  }
  if (this->timed_out_)
    return false;
  release_line(this->sda_pin_);
  delayMicroseconds(this->half_period_us_);
  if (!this->release_scl_())
    return false;
  delayMicroseconds(this->half_period_us_);
  bool ack = !this->sda_pin_->digital_read();
  pull_line_low(this->scl_pin_);
  return ack;
}

uint8_t SoftwareI2CBus::read_byte_(bool ack) {
  uint8_t data = 0;
  release_line(this->sda_pin_);
  for (uint8_t i = 0; i < 8 && !this->timed_out_; i++) {
    delayMicroseconds(this->half_period_us_);
    this->release_scl_();
    delayMicroseconds(this->half_period_us_);
    data = (data << 1) | (this->sda_pin_->digital_read() ? 1 : 0);
    pull_line_low(this->scl_pin_);
  }
  if (this->timed_out_)
    return data;
  if (ack) {
    pull_line_low(this->sda_pin_);
  }
  delayMicroseconds(this->half_period_us_);
  this->release_scl_();
  delayMicroseconds(this->half_period_us_);
  pull_line_low(this->scl_pin_);
  release_line(this->sda_pin_);
  return data;
}

ErrorCode SoftwareI2CBus::write_readv(uint8_t address, const uint8_t *write_buffer, size_t write_count,
                                      uint8_t *read_buffer, size_t read_count) {
  if (!this->initialized_) {
    ESP_LOGD(TAG, "i2c bus not initialized!");
    return ERROR_NOT_INITIALIZED;
  }
  // After a failed recovery the bus stays blocked until a later attempt frees both lines
  if (this->bus_stuck_ && !this->recover_()) {
    return ERROR_TIMEOUT;
  }
  this->bus_stuck_ = false;

  ESP_LOGV_HEX(TAG, write_buffer, write_count, I2C_MAX_LOG_BYTES, "0x%02X TX %s", address);

  this->timed_out_ = false;
  ErrorCode err = ERROR_OK;
  this->start_();
  // An empty transfer only addresses the device (used by the bus scan)
  if (write_count != 0 || read_count == 0) {
    if (!this->write_byte_(address << 1)) {
      err = ERROR_NOT_ACKNOWLEDGED;
    }
    for (size_t i = 0; err == ERROR_OK && i < write_count; i++) {
      if (!this->write_byte_(write_buffer[i])) {
        ESP_LOGVV(TAG, "TX of byte %u to 0x%02X not acknowledged", (unsigned) i, address);
        err = ERROR_NOT_ACKNOWLEDGED;
      }
    }
  }
  if (err == ERROR_OK && read_count != 0) {
    if (write_count != 0) {
      this->start_();  // repeated start
    }
    if (!this->write_byte_((address << 1) | 1)) {
      err = ERROR_NOT_ACKNOWLEDGED;
    }
    for (size_t i = 0; err == ERROR_OK && !this->timed_out_ && i < read_count; i++) {
      read_buffer[i] = this->read_byte_(i + 1 != read_count);
    }
  }
  this->stop_();

  if (this->timed_out_) {
    ESP_LOGVV(TAG, "SCL held low by 0x%02X for more than %" PRIu32 " us", address, this->timeout_);
    this->bus_stuck_ = !this->recover_();
    return ERROR_TIMEOUT;
  }
  return err;
}

}  // namespace esphome::i2c

#endif  // USE_LIBRETINY
