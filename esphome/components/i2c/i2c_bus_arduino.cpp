#if defined(USE_ARDUINO) && !defined(USE_ESP32) && !defined(USE_LIBRETINY)

#include "i2c_bus_arduino.h"
#include <Arduino.h>
#include <cstring>
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::i2c {

ESPHOME_LOG_TAG(TAG, "i2c");

// Maximum bytes to log in hex format (truncates larger transfers)
static constexpr size_t I2C_MAX_LOG_BYTES = 32;

void ArduinoI2CBus::setup() {
  this->recover_();

#if defined(USE_ESP8266)
  this->wire_ = &Wire;  // TwoWire state is static on ESP8266, so every bus shares the global instance
#elif defined(USE_RP2)
  // Select Wire instance based on pin assignment, not definition order.
  // I2C controller = (gpio / 2) % 2: even pairs (0-1,4-5,...) → I2C0, odd pairs (2-3,6-7,...) → I2C1
  // RP2040 datasheet Table 2 (section 1.4.3): https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
  // RP2350 datasheet Table 7 (section 9.4): https://datasheets.raspberrypi.com/rp2350/rp2350-datasheet.pdf
  this->wire_ = (this->sda_pin_ / 2) % 2 == 0 ? &Wire : &Wire1;
#endif

  this->set_pins_and_clock_();

  this->initialized_ = true;
  if (this->scan_) {
    ESP_LOGV(TAG, "Scanning bus for active devices");
    this->i2c_scan_();
  }
}

void ArduinoI2CBus::set_pins_and_clock_() {
#ifdef USE_RP2
  this->wire_->setSDA(this->sda_pin_);
  this->wire_->setSCL(this->scl_pin_);
  this->wire_->begin();
#else
  this->wire_->begin(static_cast<int>(this->sda_pin_), static_cast<int>(this->scl_pin_));
#endif
  if (this->timeout_ > 0) {  // if timeout specified in yaml
#if defined(USE_ESP8266)
    // https://github.com/esp8266/Arduino/blob/master/libraries/Wire/Wire.h
    this->wire_->setClockStretchLimit(this->timeout_);  // unit: us
#elif defined(USE_RP2)
    // https://github.com/earlephilhower/ArduinoCore-API/blob/e37df85425e0ac020bfad226d927f9b00d2e0fb7/api/Stream.h
    this->wire_->setTimeout(this->timeout_ / 1000);  // unit: ms
#endif
  }
  this->wire_->setClock(this->frequency_);
}

void ArduinoI2CBus::dump_config() {
  ESP_LOGCONFIG(TAG, "I2C Bus:");
  ESP_LOGCONFIG(TAG,
                "  SDA Pin: GPIO%u\n"
                "  SCL Pin: GPIO%u\n"
                "  Frequency: %u Hz",
                this->sda_pin_, this->scl_pin_, this->frequency_);
  if (this->timeout_ > 0) {
#if defined(USE_ESP8266)
    ESP_LOGCONFIG(TAG, "  Timeout: %u us", this->timeout_);
#elif defined(USE_RP2)
    ESP_LOGCONFIG(TAG, "  Timeout: %u ms", this->timeout_ / 1000);
#endif
  }
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
  this->dump_scan_results_();
}

#if defined(USE_ESP8266) && defined(I2C_BUFFER_LENGTH)
static constexpr size_t WIRE_TRANSFER_MAX = I2C_BUFFER_LENGTH;
#elif defined(USE_ESP8266)
static constexpr size_t WIRE_TRANSFER_MAX = BUFFER_LENGTH;  // older cores name it only this way
#elif defined(USE_RP2)
static constexpr size_t WIRE_TRANSFER_MAX = WIRE_BUFFER_SIZE;
#endif

ErrorCode ArduinoI2CBus::transfer_(uint8_t address, const uint8_t *prefix, size_t prefix_len,
                                   const uint8_t *write_buffer, size_t write_count, uint8_t *read_buffer,
                                   size_t read_count) {
#if defined(USE_ESP8266)
  this->set_pins_and_clock_();  // reconfigure Wire global state in case there are multiple instances
#endif
  if (!this->initialized_) {
    ESP_LOGD(TAG, "i2c bus not initialized!");
    return ERROR_NOT_INITIALIZED;
  }

  if (prefix_len != 0)
    ESP_LOGV_HEX(TAG, prefix, prefix_len, I2C_MAX_LOG_BYTES, "0x%02X TX prefix %s", address);
  ESP_LOGV_HEX(TAG, write_buffer, write_count, I2C_MAX_LOG_BYTES, "0x%02X TX %s", address);

  // Wire cannot send or receive more than its buffer in one transaction. arduino-pico leaves a transmission
  // open when a write overflows it, and every later beginTransmission() is then ignored; on ESP8266 the write
  // only fills the buffer and fails as "TX failed", and a read is clamped and fails as a timeout. Refuse both
  // up front with the real reason.
  if (prefix_len + write_count > WIRE_TRANSFER_MAX || read_count > WIRE_TRANSFER_MAX) {
    ESP_LOGV(TAG, "Transfer of %zu bytes out, %zu in exceeds the %zu byte Wire buffer", prefix_len + write_count,
             read_count, WIRE_TRANSFER_MAX);
    return ERROR_TOO_LARGE;
  }

  uint8_t status = 0;
  if (prefix_len != 0 || write_count != 0 || read_count == 0) {
    this->wire_->beginTransmission(address);
    if ((prefix_len != 0 && this->wire_->write(prefix, prefix_len) != prefix_len) ||
        this->wire_->write(write_buffer, write_count) != write_count) {
      ESP_LOGV(TAG, "TX failed");
      return ERROR_UNKNOWN;
    }
    status = this->wire_->endTransmission(read_count == 0);
  }
  if (status == 0 && read_count != 0) {
    size_t ret2 = this->wire_->requestFrom(address, read_count, true);
    if (ret2 != read_count) {
      ESP_LOGVV(TAG, "RX %u from %02X failed with error %u", read_count, address, ret2);
      return ERROR_TIMEOUT;
    }
    for (size_t j = 0; j != read_count; j++)
      read_buffer[j] = this->wire_->read();
  }
  // Avoid switch to prevent compiler-generated lookup table in RAM on ESP8266
  if (status == 0)
    return ERROR_OK;
  if (status == 1) {
    ESP_LOGVV(TAG, "TX failed: buffer not large enough");
    return ERROR_UNKNOWN;
  }
  if (status == 2 || status == 3) {
    ESP_LOGVV(TAG, "TX failed: not acknowledged: %u", status);
    return ERROR_NOT_ACKNOWLEDGED;
  }
  if (status == 5) {
    ESP_LOGVV(TAG, "TX failed: timeout");
    return ERROR_UNKNOWN;
  }
  ESP_LOGVV(TAG, "TX failed: unknown error %u", status);
  return ERROR_UNKNOWN;
}

#ifdef I2C_PORT_FREQUENCY_COUNT
ErrorCode ArduinoI2CBus::switch_frequency(uint32_t frequency) {
  if (this->frequency_ == frequency) {
    return ERROR_OK;
  }
  this->frequency_ = frequency;
  if (this->initialized_) {
    this->wire_->setClock(this->frequency_);
  }
  return ERROR_OK;
}
#endif

/// Perform I2C bus recovery, see:
/// https://www.nxp.com/docs/en/user-guide/UM10204.pdf
/// https://www.analog.com/media/en/technical-documentation/application-notes/54305147357414AN686_0.pdf
void ArduinoI2CBus::recover_() {
  ESP_LOGI(TAG, "Performing bus recovery");

  // For the upcoming operations, target for a 100kHz toggle frequency.
  // This is the maximum frequency for I2C running in standard-mode.
  // The actual frequency will be lower, because of the additional
  // function calls that are done, but that is no problem.
  const auto half_period_usec = 1000000 / 100000 / 2;

  // Activate input and pull up resistor for the SCL pin.
  pinMode(this->scl_pin_, INPUT_PULLUP);  // NOLINT

  // This should make the signal on the line HIGH. If SCL is pulled low
  // on the I2C bus however, then some device is interfering with the SCL
  // line. In that case, the I2C bus cannot be recovered.
  delayMicroseconds(half_period_usec);
  if (digitalRead(this->scl_pin_) == LOW) {  // NOLINT
    ESP_LOGE(TAG, "Recovery failed: SCL is held LOW on the bus");
    this->recovery_result_ = RECOVERY_FAILED_SCL_LOW;
    return;
  }

  // Activate input and pull up resistor for the SDA pin, so it can be read during the pulses.
  pinMode(this->sda_pin_, INPUT_PULLUP);  // NOLINT

  delayMicroseconds(half_period_usec);
  // Spec: "If the data line (SDA) is stuck LOW, send nine clock pulses."
  // Pulse only while SDA is low: a device receiving a write ACKs every ninth pulse.
  for (auto i = 0; i < 9 && digitalRead(this->sda_pin_) == LOW; i++) {  // NOLINT
    // Drive LOW. Set the level while the pin is a plain input: on RP2040 a write to an
    // INPUT_PULLUP pin drives it, which would leave SCL low if the loop never runs.
    pinMode(this->scl_pin_, INPUT);     // NOLINT
    digitalWrite(this->scl_pin_, LOW);  // NOLINT
    pinMode(this->scl_pin_, OUTPUT);    // NOLINT
    delayMicroseconds(half_period_usec);

    // Release output and activate pull up resistor to make the signal HIGH.
    pinMode(this->scl_pin_, INPUT);         // NOLINT
    pinMode(this->scl_pin_, INPUT_PULLUP);  // NOLINT
    delayMicroseconds(half_period_usec);

    // When SCL is kept LOW at this point, we might be looking at a device
    // that applies clock stretching. Wait for the release of the SCL line,
    // but not forever. There is no specification for the maximum allowed
    // time. We yield and reset the WDT, so as to avoid triggering reset.
    // No point in trying to recover the bus by forcing a uC reset. Bus
    // should recover in a few ms or less else not likely to recovery at
    // all.
    auto wait = 250;
    while (wait-- && digitalRead(this->scl_pin_) == LOW) {  // NOLINT
      App.feed_wdt();
      delayMicroseconds(half_period_usec * 2);
    }
    if (digitalRead(this->scl_pin_) == LOW) {  // NOLINT
      ESP_LOGE(TAG, "Recovery failed: SCL is held LOW during clock pulse cycle");
      this->recovery_result_ = RECOVERY_FAILED_SCL_LOW;
      return;
    }
  }

  // A device still holding SDA after nine pulses cannot be freed here.
  if (digitalRead(this->sda_pin_) == LOW) {  // NOLINT
    ESP_LOGE(TAG, "Recovery failed: SDA is held LOW after clock pulse cycle");
    this->recovery_result_ = RECOVERY_FAILED_SDA_LOW;
    return;
  }

  // From the specification:
  // "I2C-bus compatible devices must reset their bus logic on receipt of
  //  a START or repeated START condition such that they all anticipate
  //  the sending of a target address, even if these START conditions are
  //  not positioned according to the proper format."
  // SDA high does not mean the device is idle (it may be mid-byte); the START resets it.
  // SCL and SDA are already high at this point, so we can generate a START
  // condition by making the SDA signal LOW.
  delayMicroseconds(half_period_usec);
  pinMode(this->sda_pin_, INPUT);     // NOLINT
  digitalWrite(this->sda_pin_, LOW);  // NOLINT
  pinMode(this->sda_pin_, OUTPUT);    // NOLINT

  // From the specification:
  // "A START condition immediately followed by a STOP condition (void
  //  message) is an illegal format. Many devices however are designed to
  //  operate properly under this condition."
  // Finally, we'll bring the I2C bus into a starting state by generating
  // a STOP condition.
  delayMicroseconds(half_period_usec);
  pinMode(this->sda_pin_, INPUT);         // NOLINT
  pinMode(this->sda_pin_, INPUT_PULLUP);  // NOLINT

  this->recovery_result_ = RECOVERY_COMPLETED;
}
}  // namespace esphome::i2c

#endif  // defined(USE_ARDUINO) && !defined(USE_ESP32)
