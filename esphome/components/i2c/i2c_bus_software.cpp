#ifdef USE_LIBRETINY

#include "i2c_bus_software.h"
#include <Arduino.h>
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::i2c {

ESPHOME_LOG_TAG(TAG, "i2c.software");

// Maximum bytes to log in hex format (truncates larger transfers)
static constexpr size_t I2C_MAX_LOG_BYTES = 32;
// Clock pulses used to free a slave that is holding SDA low
static constexpr uint8_t RECOVERY_CLOCK_PULSES = 9;

// A line is released by switching the pin to input with pull-up and pulled low by driving it as output.
static inline void release_line(uint8_t pin) { pinMode(pin, INPUT_PULLUP); }
static inline void pull_line_low(uint8_t pin) {
  digitalWrite(pin, LOW);
  pinMode(pin, OUTPUT);
}

void SoftwareI2CBus::setup() {
  // Half of one clock period, rounded up, at least 1 us
  this->half_period_us_ = this->frequency_ != 0 ? (500000 + this->frequency_ - 1) / this->frequency_ : 5;
  if (this->half_period_us_ == 0)
    this->half_period_us_ = 1;
  release_line(this->sda_pin_);
  release_line(this->scl_pin_);
  this->recover_();
  this->initialized_ = true;
  if (this->scan_) {
    ESP_LOGV(TAG, "Scanning bus for active devices");
    this->i2c_scan_();
  }
}

void SoftwareI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG,
                "I2C Bus (software):\n"
                "  SDA Pin: GPIO%u\n"
                "  SCL Pin: GPIO%u\n"
                "  Frequency: %u Hz\n"
                "  Clock stretch timeout: %u us",
                this->sda_pin_, this->scl_pin_, this->frequency_, this->timeout_);
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

void SoftwareI2CBus::recover_() {
  // A slave that was interrupted in the middle of a transfer can hold SDA low; clock it out.
  for (uint8_t i = 0; i < RECOVERY_CLOCK_PULSES && digitalRead(this->sda_pin_) == LOW; i++) {
    pull_line_low(this->scl_pin_);
    delayMicroseconds(this->half_period_us_);
    release_line(this->scl_pin_);
    delayMicroseconds(this->half_period_us_);
    this->recovered_ = true;
  }
  if (digitalRead(this->sda_pin_) == LOW) {
    ESP_LOGW(TAG, "SDA is held low");
    this->recovered_ = false;
  }
}

// Release SCL and wait for it to go high, a slave may stretch the clock.
bool SoftwareI2CBus::release_scl_() {
  release_line(this->scl_pin_);
  uint32_t start = micros();
  while (digitalRead(this->scl_pin_) == LOW) {
    if (micros() - start > this->timeout_) {
      this->scl_timed_out_ = true;
      return false;
    }
  }
  return true;
}

void SoftwareI2CBus::start_() {
  // Also generates a repeated start when SCL is low and SDA is released
  release_line(this->sda_pin_);
  delayMicroseconds(this->half_period_us_);
  this->release_scl_();
  delayMicroseconds(this->half_period_us_);
  pull_line_low(this->sda_pin_);
  delayMicroseconds(this->half_period_us_);
  pull_line_low(this->scl_pin_);
}

void SoftwareI2CBus::stop_() {
  pull_line_low(this->sda_pin_);
  delayMicroseconds(this->half_period_us_);
  this->release_scl_();
  delayMicroseconds(this->half_period_us_);
  release_line(this->sda_pin_);
  delayMicroseconds(this->half_period_us_);
}

/// Shift out one byte, returns true if the slave acknowledged it.
bool SoftwareI2CBus::write_byte_(uint8_t data) {
  for (uint8_t mask = 0x80; mask != 0; mask >>= 1) {
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
  release_line(this->sda_pin_);
  delayMicroseconds(this->half_period_us_);
  this->release_scl_();
  delayMicroseconds(this->half_period_us_);
  bool ack = digitalRead(this->sda_pin_) == LOW;
  pull_line_low(this->scl_pin_);
  return ack;
}

uint8_t SoftwareI2CBus::read_byte_(bool ack) {
  uint8_t data = 0;
  release_line(this->sda_pin_);
  for (uint8_t i = 0; i < 8; i++) {
    delayMicroseconds(this->half_period_us_);
    this->release_scl_();
    delayMicroseconds(this->half_period_us_);
    data = (data << 1) | (digitalRead(this->sda_pin_) == HIGH ? 1 : 0);
    pull_line_low(this->scl_pin_);
  }
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

  ESP_LOGV_HEX(TAG, write_buffer, write_count, I2C_MAX_LOG_BYTES, "0x%02X TX %s", address);

  this->scl_timed_out_ = false;
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
        err = ERROR_UNKNOWN;
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
    for (size_t i = 0; err == ERROR_OK && i < read_count; i++) {
      read_buffer[i] = this->read_byte_(i + 1 != read_count);
    }
  }
  this->stop_();

  if (this->scl_timed_out_) {
    ESP_LOGVV(TAG, "SCL held low by 0x%02X for more than %u us", address, this->timeout_);
    this->recover_();
    return ERROR_TIMEOUT;
  }
  return err;
}

}  // namespace esphome::i2c

#endif  // USE_LIBRETINY
