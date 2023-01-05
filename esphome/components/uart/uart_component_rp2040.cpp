#ifdef USE_RP2040
#include "uart_component_rp2040.h"
#include "esphome/core/application.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include <hardware/uart.h>

#ifdef USE_LOGGER
#include "esphome/components/logger/logger.h"
#endif

namespace esphome {
namespace uart {

static const char *const TAG = "uart.arduino_rp2040";

uint16_t RP2040UartComponent::get_config() {
  uint16_t config = 0;

  if (this->parity_ == UART_CONFIG_PARITY_NONE) {
    config |= UART_PARITY_NONE;
  } else if (this->parity_ == UART_CONFIG_PARITY_EVEN) {
    config |= UART_PARITY_EVEN;
  } else if (this->parity_ == UART_CONFIG_PARITY_ODD) {
    config |= UART_PARITY_ODD;
  }

  switch (this->data_bits_) {
    case 5:
      config |= SERIAL_DATA_5;
      break;
    case 6:
      config |= SERIAL_DATA_6;
      break;
    case 7:
      config |= SERIAL_DATA_7;
      break;
    case 8:
      config |= SERIAL_DATA_8;
      break;
  }

  if (this->stop_bits_ == 1) {
    config |= SERIAL_STOP_BIT_1;
  } else {
    config |= SERIAL_STOP_BIT_2;
  }

  return config;
}

void RP2040UartComponent::setup() {
  ESP_LOGCONFIG(TAG, "Setting up UART bus...");

  uint16_t config = get_config();

  constexpr uint32_t valid_tx_uart_0 = __bitset({0, 12, 16, 28});
  constexpr uint32_t valid_tx_uart_1 = __bitset({4, 8, 20, 24});

  constexpr uint32_t valid_rx_uart_0 = __bitset({1, 13, 17, 29});
  constexpr uint32_t valid_rx_uart_1 = __bitset({5, 9, 21, 25});

  int8_t tx_hw = -1;
  int8_t rx_hw = -1;

  if (this->tx_pin_ != nullptr) {
    if (this->tx_pin_->is_inverted()) {
      ESP_LOGD(TAG, "An inverted TX pin %u can only be used with SerialPIO", this->tx_pin_->get_pin());
    } else {
      if (((1 << this->tx_pin_->get_pin()) & valid_tx_uart_0) != 0) {
        tx_hw = 0;
      } else if (((1 << this->tx_pin_->get_pin()) & valid_tx_uart_1) != 0) {
        tx_hw = 1;
      } else {
        ESP_LOGD(TAG, "TX pin %u can only be used with SerialPIO", this->tx_pin_->get_pin());
      }
    }
  }

  if (this->rx_pin_ != nullptr) {
    if (this->rx_pin_->is_inverted()) {
      ESP_LOGD(TAG, "An inverted RX pin %u can only be used with SerialPIO", this->rx_pin_->get_pin());
    } else {
      if (((1 << this->rx_pin_->get_pin()) & valid_rx_uart_0) != 0) {
        rx_hw = 0;
      } else if (((1 << this->rx_pin_->get_pin()) & valid_rx_uart_1) != 0) {
        rx_hw = 1;
      } else {
        ESP_LOGD(TAG, "RX pin %u can only be used with SerialPIO", this->rx_pin_->get_pin());
      }
    }
  }

#ifdef USE_LOGGER
  if (tx_hw == rx_hw && logger::global_logger->get_uart() == tx_hw) {
    ESP_LOGD(TAG, "Using SerialPIO as UART%d is taken by the logger", tx_hw);
    tx_hw = -1;
    rx_hw = -1;
  }
#endif

  if (tx_hw == -1 || rx_hw == -1 || tx_hw != rx_hw) {
    ESP_LOGV(TAG, "Using SerialPIO");
    pin_size_t tx = this->tx_pin_ == nullptr ? SerialPIO::NOPIN : this->tx_pin_->get_pin();
    pin_size_t rx = this->rx_pin_ == nullptr ? SerialPIO::NOPIN : this->rx_pin_->get_pin();
    auto *serial = new SerialPIO(tx, rx, this->rx_buffer_size_);  // NOLINT(cppcoreguidelines-owning-memory)
    serial->begin(this->baud_rate_, config);
    if (this->tx_pin_ != nullptr && this->tx_pin_->is_inverted())
      gpio_set_outover(tx, GPIO_OVERRIDE_INVERT);
    if (this->rx_pin_ != nullptr && this->rx_pin_->is_inverted())
      gpio_set_inover(rx, GPIO_OVERRIDE_INVERT);
    this->serial_ = serial;
  } else {
    ESP_LOGV(TAG, "Using Hardware Serial");
    SerialUART *serial;
    if (tx_hw == 0) {
      serial = &Serial1;
    } else {
      serial = &Serial2;
    }
    serial->setTX(this->tx_pin_->get_pin());
    serial->setRX(this->rx_pin_->get_pin());
    serial->setFIFOSize(this->rx_buffer_size_);
    serial->begin(this->baud_rate_, config);
    this->serial_ = serial;
    this->hw_serial_ = true;
  }
}

void RP2040UartComponent::dump_config() {
  ESP_LOGCONFIG(TAG, "UART Bus:");
  LOG_PIN("  TX Pin: ", tx_pin_);
  LOG_PIN("  RX Pin: ", rx_pin_);
  if (this->rx_pin_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  RX Buffer Size: %u", this->rx_buffer_size_);
  }
  ESP_LOGCONFIG(TAG, "  Baud Rate: %u baud", this->baud_rate_);
  ESP_LOGCONFIG(TAG, "  Data Bits: %u", this->data_bits_);
  ESP_LOGCONFIG(TAG, "  Parity: %s", LOG_STR_ARG(parity_to_str(this->parity_)));
  ESP_LOGCONFIG(TAG, "  Stop bits: %u", this->stop_bits_);
  if (this->hw_serial_) {
    ESP_LOGCONFIG(TAG, "  Using hardware serial");
  } else {
    ESP_LOGCONFIG(TAG, "  Using SerialPIO");
  }
}

void RP2040UartComponent::write_array(const uint8_t *data, size_t len) {
  this->serial_->write(data, len);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_TX, data[i]);
  }
#endif
}
bool RP2040UartComponent::peek_byte(uint8_t *data) {
  if (!this->check_read_timeout_())
    return false;
  *data = this->serial_->peek();
  return true;
}
bool RP2040UartComponent::read_array(uint8_t *data, size_t len) {
  if (!this->check_read_timeout_(len))
    return false;
  this->serial_->readBytes(data, len);
#ifdef USE_UART_DEBUGGER
  for (size_t i = 0; i < len; i++) {
    this->debug_callback_.call(UART_DIRECTION_RX, data[i]);
  }
#endif
  return true;
}
int RP2040UartComponent::available() { return this->serial_->available(); }
void RP2040UartComponent::flush() {
  ESP_LOGVV(TAG, "    Flushing...");
  this->serial_->flush();
}

}  // namespace uart
}  // namespace esphome

#endif  // USE_RP2040
