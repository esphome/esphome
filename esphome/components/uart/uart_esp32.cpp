#ifdef ARDUINO_ARCH_ESP32
#include "uart.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"

namespace esphome {
namespace uart {
static const char *TAG = "uart_esp32";
uint8_t next_uart_num = 1;

static const uint32_t UART_PARITY_EVEN = 0 << 0;
static const uint32_t UART_PARITY_ODD = 1 << 0;
static const uint32_t UART_PARITY_EN = 1 << 1;
static const uint32_t UART_NB_BIT_5 = 0 << 2;
static const uint32_t UART_NB_BIT_6 = 1 << 2;
static const uint32_t UART_NB_BIT_7 = 2 << 2;
static const uint32_t UART_NB_BIT_8 = 3 << 2;
static const uint32_t UART_NB_STOP_BIT_1 = 1 << 4;
static const uint32_t UART_NB_STOP_BIT_2 = 3 << 4;
static const uint32_t UART_TICK_APB_CLOCK = 1 << 27;

uint32_t UARTComponent::get_config() {
  uint32_t config = 0;

  /*
   * All bits numbers below come from
   * framework-arduinoespressif32/cores/esp32/esp32-hal-uart.h
   * And more specifically conf0 union in uart_dev_t.
   *
   * Below is bit used from conf0 union.
   * <name>:<bits position>  <values>
   * parity:0                0:even 1:odd
   * parity_en:1             Set this bit to enable uart parity check.
   * bit_num:2-4             0:5bits 1:6bits 2:7bits 3:8bits
   * stop_bit_num:4-6        stop bit. 1:1bit  2:1.5bits  3:2bits
   * tick_ref_always_on:27   select the clock.1：apb clock：ref_tick
   */

  if (this->parity_ == UART_CONFIG_PARITY_EVEN)
    config |= UART_PARITY_EVEN | UART_PARITY_EN;
  else if (this->parity_ == UART_CONFIG_PARITY_ODD)
    config |= UART_PARITY_ODD | UART_PARITY_EN;

  switch (this->data_bits_) {
    case 5:
      config |= UART_NB_BIT_5;
      break;
    case 6:
      config |= UART_NB_BIT_6;
      break;
    case 7:
      config |= UART_NB_BIT_7;
      break;
    case 8:
      config |= UART_NB_BIT_8;
      break;
  }

  if (this->stop_bits_ == 1)
    config |= UART_NB_STOP_BIT_1;
  else
    config |= UART_NB_STOP_BIT_2;

  config |= UART_TICK_APB_CLOCK;

  return config;
}

void UARTComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up UART...");
  // Use Arduino HardwareSerial UARTs if all used pins match the ones
  // preconfigured by the platform. For example if RX disabled but TX pin
  // is 1 we still want to use Serial.
  if (this->tx_pin_.value_or(1) == 1 && this->rx_pin_.value_or(3) == 3) {
    this->hw_serial_ = &Serial;
  } else {
    this->hw_serial_ = new HardwareSerial(next_uart_num++);
  }
  int8_t tx = this->tx_pin_.has_value() ? *this->tx_pin_ : -1;
  int8_t rx = this->rx_pin_.has_value() ? *this->rx_pin_ : -1;
  this->hw_serial_->begin(this->baud_rate_, get_config(), rx, tx);
  this->hw_serial_->setRxBufferSize(this->rx_buffer_size_);
}

void UARTComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Bus:");
  if (this->tx_pin_.has_value()) {
    ESP_LOGCONFIG(TAG, "  TX Pin: GPIO%d", *this->tx_pin_);
  }
  if (this->rx_pin_.has_value()) {
    ESP_LOGCONFIG(TAG, "  RX Pin: GPIO%d", *this->rx_pin_);
    ESP_LOGCONFIG(TAG, "  RX Buffer Size: %u", this->rx_buffer_size_);
  }
  ESP_LOGCONFIG(TAG, "  Baud Rate: %u baud", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Data Bits: %u", this->data_bits_);
  ESP_LOGCONFIG(TAG, "  Parity: %s", parity_to_str(this->parity_));
  ESP_LOGCONFIG(TAG, "  Stop bits: %u", this->stop_bits_);
  this->check_logger_conflict_();
}

void UARTComponent::write_byte(uint8_t data) {
  this->hw_serial_->write(data);
  ESP_LOGVV(TAG, "    Wrote 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(data), data);
}
void UARTComponent::write_array(const uint8_t *data, size_t len) {
  this->hw_serial_->write(data, len);
  for (size_t i = 0; i < len; i++) {
    ESP_LOGVV(TAG, "    Wrote 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(data[i]), data[i]);
  }
}
void UARTComponent::write_str(const char *str) {
  this->hw_serial_->write(str);
  ESP_LOGVV(TAG, "    Wrote \"%s\"", str);
}
bool UARTComponent::read_byte(uint8_t *data) {
  if (!this->check_read_timeout_())
    return false;
  *data = this->hw_serial_->read();
  ESP_LOGVV(TAG, "    Read 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(*data), *data);
  return true;
}
bool UARTComponent::peek_byte(uint8_t *data) {
  if (!this->check_read_timeout_())
    return false;
  *data = this->hw_serial_->peek();
  return true;
}
bool UARTComponent::read_array(uint8_t *data, size_t len) {
  if (!this->check_read_timeout_(len))
    return false;
  this->hw_serial_->readBytes(data, len);
  for (size_t i = 0; i < len; i++) {
    ESP_LOGVV(TAG, "    Read 0b" BYTE_TO_BINARY_PATTERN " (0x%02X)", BYTE_TO_BINARY(data[i]), data[i]);
  }

  return true;
}
bool UARTComponent::check_read_timeout_(size_t len) {
  if (this->available() >= len)
    return true;

  uint32_t start_time = millis();
  while (this->available() < len) {
    if (millis() - start_time > 1000) {
      ESP_LOGE(TAG, "Reading from UART timed out at byte %u!", this->available());
      return false;
    }
    yield();
  }
  return true;
}
int UARTComponent::available() { return this->hw_serial_->available(); }
void UARTComponent::flush() {
  ESP_LOGVV(TAG, "    Flushing...");
  this->hw_serial_->flush();
}

}  // namespace uart
}  // namespace esphome
#endif  // ARDUINO_ARCH_ESP32
