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
  switch (this->recovery_result_) {
    case RECOVERY_COMPLETED:
      ESP_LOGCONFIG(TAG, "  Recovery: bus successfully recovered");
      break;
    case RECOVERY_FAILED_SCL_LOW:
      ESP_LOGCONFIG(TAG, "  Recovery: failed, SCL is held low on the bus");
      break;
    case RECOVERY_FAILED_SDA_LOW:
      ESP_LOGCONFIG(TAG, "  Recovery: failed, SDA is held low on the bus");
      break;
  }
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

/// Perform I2C bus recovery, see:
/// https://www.nxp.com/docs/en/user-guide/UM10204.pdf
/// https://www.analog.com/media/en/technical-documentation/application-notes/54305147357414AN686_0.pdf
void ArduinoI2CBus::recover_() {
  ESP_LOGI(TAG, "Performing I2C bus recovery");

  // For the upcoming operations, target for a 100kHz toggle frequency.
  // This is the maximum frequency for I2C running in standard-mode.
  // The actual frequency will be lower, because of the additional
  // function calls that are done, but that is no problem.
  const auto half_period_usec = 1000000 / 100000 / 2;

  // Activate input and pull up resistor for the SCL pin. This should make
  // the signal on the line HIGH. If SCL is pulled low on the I2C bus
  // however, then some device is interfering with the SCL line. In that
  // case, the I2C bus cannot be recovered.
  pinMode(scl_pin_, INPUT_PULLUP);     // NOLINT
  if (digitalRead(scl_pin_) == LOW) {  // NOLINT
    ESP_LOGE(TAG, "Recovery failed: SCL is held LOW on the I2C bus");
    recovery_result_ = RECOVERY_FAILED_SCL_LOW;
    return;
  }

  // From the specification:
  // "If the data line (SDA) is stuck LOW, send nine clock pulses. The
  //  device that held the bus LOW should release it sometime within
  //  those nine clocks."
  // We don't really have to detect if SDA is stuck low. We'll simply send
  // nine clock pulses here, just in case SDA is stuck. Actual checks on
  // the SDA line status will be done after the clock pulses.

  // Make sure that switching to output mode will make SCL low, just in
  // case other code has setup the pin to output a HIGH signal.
  digitalWrite(scl_pin_, LOW);  // NOLINT

  // Activate input and pull resistor for the SDA pin, so after the clock
  // pulse cycle we can verify that SDA is pulled high. Also make sure
  // that switching to output mode will make SDA low.
  pinMode(sda_pin_, INPUT_PULLUP);  // NOLINT
  digitalWrite(sda_pin_, LOW);      // NOLINT

  ESP_LOGI(TAG, "Sending 9 clock pulses to drain any stuck device output");
  delayMicroseconds(half_period_usec);
  for (auto i = 0; i < 9; i++) {
    // Release pull up resistor and switch to output to make the signal LOW.
    pinMode(scl_pin_, INPUT);   // NOLINT
    pinMode(scl_pin_, OUTPUT);  // NOLINT
    delayMicroseconds(half_period_usec);

    // Release output and activate pull up resistor to make the signal HIGH.
    pinMode(scl_pin_, INPUT);         // NOLINT
    pinMode(scl_pin_, INPUT_PULLUP);  // NOLINT
    delayMicroseconds(half_period_usec);

    // When SCL is kept LOW at this point, we might be looking at a device
    // that applies clock stretching. Wait for the release of the SCL line,
    // but not forever. There is no specification for the maximum allowed
    // time. We'll stick to 500ms here.
    auto wait = 20;
    while (wait-- && digitalRead(scl_pin_) == LOW) {  // NOLINT
      delay(25);
    }
    if (digitalRead(scl_pin_) == LOW) {  // NOLINT
      ESP_LOGE(TAG, "Recovery failed: SCL is held LOW during clock pulse cycle");
      recovery_result_ = RECOVERY_FAILED_SCL_LOW;
      return;
    }
  }

  // By now, any stuck device ought to have sent all remaining bits of its
  // transation, meaning that it should have freed up the SDA line, resulting
  // in SDA being pulled up.
  if (digitalRead(sda_pin_) == LOW) {  // NOLINT
    ESP_LOGE(TAG, "Recovery failed: SDA is held LOW after clock pulse cycle");
    recovery_result_ = RECOVERY_FAILED_SDA_LOW;
    return;
  }

  // From the specification:
  // "I2C-bus compatible devices must reset their bus logic on receipt of
  //  a START or repeated START condition such that they all anticipate
  //  the sending of a target address, even if these START conditions are
  //  not positioned according to the proper format."
  // While the 9 clock pulses from above might have drained all bits of a
  // single byte within a transaction, a device might have more bytes to
  // transmit. So here we'll generate a START condition to snap the device
  // out of this state.
  // SCL and SDA are already high at this point, so we can generate a START
  // condition by making the SDA signal LOW.
  ESP_LOGI(TAG, "Generate START condition to reset bus logic of I2C devices");
  pinMode(sda_pin_, INPUT);   // NOLINT
  pinMode(sda_pin_, OUTPUT);  // NOLINT
  delayMicroseconds(half_period_usec);

  // From the specification:
  // "A START condition immediately followed by a STOP condition (void
  //  message) is an illegal format. Many devices however are designed to
  //  operate properly under this condition."
  // Finally, we'll bring the I2C bus into a starting state by generating
  // a STOP condition.
  ESP_LOGI(TAG, "Generate STOP condition to finalize recovery");
  pinMode(sda_pin_, INPUT);         // NOLINT
  pinMode(sda_pin_, INPUT_PULLUP);  // NOLINT

  recovery_result_ = RECOVERY_COMPLETED;
}
}  // namespace i2c
}  // namespace esphome

#endif  // USE_ESP_IDF
