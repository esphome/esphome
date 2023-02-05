#pragma once

#ifdef USE_ESP_IDF
#include "esphome/components/uart/uart_component_esp_idf.h"
#include "esphome/core/log.h"
#endif

#ifdef USE_ESP32_FRAMEWORK_ARDUINO
#include "esphome/components/uart/uart_component_esp32_arduino.h"
#include <HardwareSerial.h>
#endif

#ifdef USE_ESP8266
#include "esphome/components/uart/uart_component_esp8266.h"
#endif

namespace esphome {
namespace iec62056 {

static const uint32_t TIMEOUT = 20;  // default value in uart implementation is 100ms

#ifdef USE_ESP32_FRAMEWORK_ARDUINO

class IEC62056UART final : public uart::ESP32ArduinoUARTComponent {
 public:
  IEC62056UART(uart::ESP32ArduinoUARTComponent const &uart) : uart_(uart), hw_(uart.*(&IEC62056UART::hw_serial_)) {}

  // Reconfigure baudrate
  void update_baudrate(uint32_t baudrate) { this->hw_->updateBaudRate(baudrate); }

  /// @brief Reads one byte. Uses 20ms inter-character timeout.
  /// @param data Pointer to one byte buffer to store data
  /// @retval true byte received
  /// @retval false no data
  /// @remarks
  /// Default @c read_byte() function waits 100 ms when no data in input buffer.
  /// This increase time spent in @c loop() function above accepted value (50ms).
  /// Using the following implementation solves this problem. Higher level function
  /// (@ref IEC62056Component::receive_frame_()) implements it's own timeout and can
  /// properly handle fragmented packets.
  bool read_one_byte(uint8_t *data) {
    if (!this->check_read_timeout_quick_(1))
      return false;
    this->hw_->readBytes(data, 1);
    return true;
  }

 protected:
  /// @brief Helper function for @ref read_one_byte()
  /// @remarks
  /// Uses 20ms timeout instead of default 100ms.
  bool check_read_timeout_quick_(size_t len) {
    if (this->hw_->available() >= int(len))
      return true;

    uint32_t start_time = millis();
    while (this->hw_->available() < int(len)) {
      if (millis() - start_time > TIMEOUT) {
        return false;
      }
      yield();
    }
    return true;
  }

  uart::ESP32ArduinoUARTComponent const &uart_;
  HardwareSerial *const hw_;
};
#endif

#ifdef USE_ESP8266

class XSoftSerial : public uart::ESP8266SoftwareSerial {
 public:
  void set_bit_time(uint32_t bt) { bit_time_ = bt; }
};

class IEC62056UART final : public uart::ESP8266UartComponent {
 public:
  IEC62056UART(uart::ESP8266UartComponent const &uart)
      : uart_(uart), hw_(uart.*(&IEC62056UART::hw_serial_)), sw_(uart.*(&IEC62056UART::sw_serial_)) {}

  void update_baudrate(uint32_t baudrate) {
    if (this->hw_ != nullptr) {
      this->hw_->updateBaudRate(baudrate);
    } else if (baudrate > 0) {
      ((XSoftSerial *) sw_)->set_bit_time(F_CPU / baudrate);
    }
  }

  bool read_one_byte(uint8_t *data) {
    if (this->hw_ != nullptr) {
      if (!this->check_read_timeout_quick_(1))
        return false;
      this->hw_->readBytes(data, 1);
    } else {
      if (sw_->available() < 1)
        return false;
      assert(this->sw_ != nullptr);
      optional<uint8_t> b = this->sw_->read_byte();
      if (b) {
        *data = *b;
      } else {
        return false;
      }
    }
    return true;
  }

 protected:
  bool check_read_timeout_quick_(size_t len) {
    if (this->hw_->available() >= int(len))
      return true;

    uint32_t start_time = millis();
    while (this->hw_->available() < int(len)) {
      if (millis() - start_time > TIMEOUT) {
        return false;
      }
      yield();
    }
    return true;
  }

  uart::ESP8266UartComponent const &uart_;
  HardwareSerial *const hw_;               // hardware Serial
  uart::ESP8266SoftwareSerial *const sw_;  // software serial
};
#endif

#ifdef USE_ESP_IDF
class IEC62056UART final : public uart::IDFUARTComponent {
 public:
  IEC62056UART(uart::IDFUARTComponent &uart)
      : uart_(uart), iuart_num_(uart.*(&IEC62056UART::uart_num_)), ilock_(uart.*(&IEC62056UART::lock_)) {}

  // Reconfigure baudrate
  void update_baudrate(uint32_t baudrate) {
    xSemaphoreTake(ilock_, portMAX_DELAY);
    uart_set_baudrate(iuart_num_, baudrate);
    xSemaphoreGive(ilock_);
  }

  bool read_one_byte(uint8_t *data) { return read_array_quick_(data, 1); }

 protected:
  bool check_read_timeout_quick_(size_t len) {
    if (uart_.available() >= int(len))
      return true;

    uint32_t start_time = millis();
    while (uart_.available() < int(len)) {
      if (millis() - start_time > TIMEOUT) {
        return false;
      }
      yield();
    }
    return true;
  }

  bool read_array_quick_(uint8_t *data, size_t len) {
    size_t length_to_read = len;
    if (!this->check_read_timeout_quick_(len))
      return false;
    xSemaphoreTake(this->ilock_, portMAX_DELAY);
    if (this->has_peek_) {
      length_to_read--;
      *data = this->peek_byte_;
      data++;
      this->has_peek_ = false;
    }
    if (length_to_read > 0)
      uart_read_bytes(this->iuart_num_, data, length_to_read, 20 / portTICK_RATE_MS);
    xSemaphoreGive(this->ilock_);

    return true;
  }

  uart::IDFUARTComponent &uart_;
  uart_port_t iuart_num_;
  SemaphoreHandle_t &ilock_;
};
#endif

}  // namespace iec62056
}  // namespace esphome
