#include "uart_manager.h"

namespace esphome::libretiny {

void UartManager::deinit_all() {
  lt_log_disable();
  for (auto &uart_opt : uarts_) {
    if (uart_opt.has_value()) {
      uart_opt->hw_serial.end();
    }
  }
}

const FixedVector<pin_size_t> &UartManager::get_tx_pins_for_uart(const uint8_t uart_number) const {
  auto *uart = get_uart_by_number_(uart_number);
  if (!uart) {
    return get_empty_pins();
  }
  return uart->tx_pins;
}

const FixedVector<pin_size_t> &UartManager::get_rx_pins_for_uart(const uint8_t uart_number) const {
  auto *uart = get_uart_by_number_(uart_number);
  if (!uart) {
    return get_empty_pins();
  }
  return uart->rx_pins;
}

HardwareSerial *UartManager::get_hw_serial_by_number(const uint8_t uart_number) {
  auto *uart = get_uart_by_number_(uart_number);
  if (!uart) {
    return nullptr;
  }
  return &(uart->hw_serial);
}

bool UartManager::validate_pins(const uint8_t uart_number, const uint8_t rx_pin, const uint8_t tx_pin) const {
  auto *uart = get_uart_by_number_(uart_number);
  if (!uart) {
    return false;
  }
  return uart->validate_pins(rx_pin, tx_pin);
}

/**
 * @brief Initialize UART without changing pins
 *
 * @param uart_number Hardware UART number (0-2)
 * @param baud_rate Baud rate to use
 * @param config Configuration (data bits, parity, stop bits)
 * @return true on success
 */
bool UartManager::init_uart(const uint8_t uart_number, const uint32_t baud_rate, const uint16_t config) {
  auto *uart = get_uart_by_number_(uart_number);
  if (!uart) {
    return false;
  }

  uart->hw_serial.begin(baud_rate, config);
  return true;
}

/**
 * @brief
 *
 * @param uart_number Hardware UART number (0-2)
 * @param baud_rate Baud rate to use
 * @param config Configuration (data bits, parity, stop bits)
 * @param rx_pin pin number to use for RX
 * @param tx_pin pin number to use for TX
 * @return true on success
 */
bool UartManager::init_uart(const uint8_t uart_number, const uint32_t baud_rate, const uint16_t config,
                            const uint8_t rx_pin, const uint8_t tx_pin) {
  auto *uart = get_uart_by_number_(uart_number);
  if (!uart) {
    return false;
  }

  if (!uart->validate_pins(rx_pin, tx_pin)) {
    return false;
  }

  uart->hw_serial.begin(baud_rate, config, rx_pin, tx_pin);
  return true;
}

bool UartManager::init_uart_for_logger(const uint8_t uart_number, const uint32_t baud_rate) {
  auto *uart = get_uart_by_number_(uart_number);
  if (!uart) {
    return false;
  }

  uart->hw_serial.begin(baud_rate, SERIAL_8N1, uart->get_logger_rx_pin(), uart->get_logger_tx_pin());
  lt_log_set_port(uart_number);
  return true;
}

void UartManager::deinit_uart(const uint8_t uart_number) {
  auto *uart = get_uart_by_number_(uart_number);
  if (!uart) {
    return;
  }

  uart->hw_serial.end();
}

UartManager::UartInfo *UartManager::get_uart_by_number_(const uint8_t uart_number) {
  if ((uart_number >= S_MAX_UARTS) || (!uarts_[uart_number].has_value())) {
    return nullptr;
  }
  return &uarts_[uart_number].value();
}

const UartManager::UartInfo *UartManager::get_uart_by_number_(const uint8_t uart_number) const {
  if ((uart_number >= S_MAX_UARTS) || (!uarts_[uart_number].has_value())) {
    return nullptr;
  }
  return &uarts_[uart_number].value();
}

}  // namespace esphome::libretiny
