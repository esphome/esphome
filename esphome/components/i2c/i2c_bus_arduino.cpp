#ifdef USE_ARDUINO

#include "i2c_bus_arduino.h"
#include "esphome/core/log.h"
#include <Arduino.h>
#include <cstring>

namespace esphome {
namespace i2c {

static const char *const TAG = "i2c.arduino";

void ArduinoI2CBus::setup() {
  recover_();
#ifdef USE_ESP32
  static uint8_t next_bus_num = 0;
  if (next_bus_num == 0)
    wire_ = &Wire;
  else
    wire_ = new TwoWire(next_bus_num);  // NOLINT(cppcoreguidelines-owning-memory)
  next_bus_num++;
#else
  wire_ = &Wire;  // NOLINT(cppcoreguidelines-prefer-member-initializer)
#endif

  wire_->begin(sda_pin_, scl_pin_);
  wire_->setClock(frequency_);
  initialized_ = true;
}
void ArduinoI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus:");
  ESP_LOGCONFIG(TAG, "  SDA Pin: GPIO%u", this->sda_pin_);
  ESP_LOGCONFIG(TAG, "  SCL Pin: GPIO%u", this->scl_pin_);
  ESP_LOGCONFIG(TAG, "  Frequency: %u Hz", this->frequency_);
  if (this->scan_) {
    ESP_LOGI(TAG, "Scanning i2c bus for active devices...");
    uint8_t found = 0;
    for (uint8_t address = 8; address < 120; address++) {
      auto err = writev(address, nullptr, 0);
      if (err == ERROR_OK) {
        ESP_LOGI(TAG, "Found i2c device at address 0x%02X", address);
        found++;
      } else if (err == ERROR_UNKNOWN) {
        ESP_LOGI(TAG, "Unknown error at address 0x%02X", address);
      }
    }
    if (found == 0) {
      ESP_LOGI(TAG, "Found no i2c devices!");
    }
  }
}
ErrorCode ArduinoI2CBus::readv(uint8_t address, ReadBuffer *buffers, size_t cnt) {
  if (!initialized_)
    return ERROR_NOT_INITIALIZED;
  size_t to_request = 0;
  for (size_t i = 0; i < cnt; i++)
    to_request += buffers[i].len;
  size_t ret = wire_->requestFrom((int) address, (int) to_request, 1);
  if (ret != to_request) {
    return ERROR_TIMEOUT;
  }
  for (size_t i = 0; i < cnt; i++) {
    const auto &buf = buffers[i];
    for (size_t j = 0; j < buf.len; j++)
      buf.data[j] = wire_->read();
  }
  return ERROR_OK;
}
ErrorCode ArduinoI2CBus::writev(uint8_t address, WriteBuffer *buffers, size_t cnt) {
  if (!initialized_)
    return ERROR_NOT_INITIALIZED;

  wire_->beginTransmission(address);
  for (size_t i = 0; i < cnt; i++) {
    const auto &buf = buffers[i];
    if (buf.len == 0)
      continue;
    size_t ret = wire_->write(buf.data, buf.len);
    if (ret != buf.len) {
      return ERROR_UNKNOWN;
    }
  }
  uint8_t status = wire_->endTransmission(true);
  if (status == 0) {
    return ERROR_OK;
  } else if (status == 1) {
    // transmit buffer not large enough
    return ERROR_UNKNOWN;
  } else if (status == 2 || status == 3) {
    return ERROR_NOT_ACKNOWLEDGED;
  }
  return ERROR_UNKNOWN;
}

void ArduinoI2CBus::recover_() {
  // Perform I2C bus recovery, see
  // https://www.analog.com/media/en/technical-documentation/application-notes/54305147357414AN686_0.pdf
  // or see the linux kernel implementation, e.g.
  // https://elixir.bootlin.com/linux/v5.14.6/source/drivers/i2c/i2c-core-base.c#L200

  // try to get about 100kHz toggle frequency
  const auto half_period_usec = 1000000 / 100000 / 2;
  const auto recover_scl_periods = 9;

  // configure scl as output
  pinMode(scl_pin_, OUTPUT);  // NOLINT

  // set scl high
  digitalWrite(scl_pin_, 1);  // NOLINT

  // in total generate 9 falling-rising edges
  for (auto i = 0; i < recover_scl_periods; i++) {
    delayMicroseconds(half_period_usec);
    digitalWrite(scl_pin_, 0);  // NOLINT
    delayMicroseconds(half_period_usec);
    digitalWrite(scl_pin_, 1);  // NOLINT
  }

  delayMicroseconds(half_period_usec);
}
}  // namespace i2c
}  // namespace esphome

#endif  // USE_ESP_IDF
