#pragma once

#ifdef USE_LIBRETINY

#include "esphome/core/helpers.h"

#include <optional>
#include <array>

namespace esphome::libretiny {

struct UartManager {
  FixedVector<uint8_t> get_available_uarts() const {
    return FixedVector<uint8_t> {
#if LT_HW_UART0
      0,
#endif
#if LT_HW_UART1
          1,
#endif
#if LT_HW_UART2
          2,
#endif
    };
  }

  void deinit_all();
  const FixedVector<pin_size_t> &get_tx_pins_for_uart(uint8_t uart_number) const;
  const FixedVector<pin_size_t> &get_rx_pins_for_uart(uint8_t uart_number) const;
  HardwareSerial *get_hw_serial_by_number(const uint8_t uart_number);

  uint8_t get_default_uart_number() const;
  bool validate_pins(uint8_t uart_number, uint8_t rx_pin, uint8_t tx_pin) const;
  bool init_uart(uint8_t uart_number, uint32_t baud_rate, uint16_t config);
  bool init_uart(uint8_t uart_number, uint32_t baud_rate, uint16_t config, uint8_t rx_pin, uint8_t tx_pin);
  bool init_uart_for_logger(uint8_t uart_number, uint32_t baud_rate);
  void deinit_uart(uint8_t uart_number);

 private:
  static const FixedVector<pin_size_t> &get_empty_pins() {
    static const FixedVector<pin_size_t> S_EMPTY_PINS;
    return S_EMPTY_PINS;
  }

  static constexpr size_t S_MAX_UARTS = 3;

  struct UartInfo {
    ::SerialClass &hw_serial;
    FixedVector<pin_size_t> tx_pins;
    FixedVector<pin_size_t> rx_pins;

    uint8_t get_logger_rx_pin() const {
      if (rx_pins.size() > 0) {
        return rx_pins[0];
      }
      return PIN_INVALID;
    };

    uint8_t get_logger_tx_pin() const {
      if (tx_pins.size() > 0) {
        return tx_pins[0];
      }
      return PIN_INVALID;
    };

    bool validate_pins(const uint8_t rx_pin, const uint8_t tx_pin) const {
      if ((rx_pin == PIN_INVALID) && (tx_pin == PIN_INVALID)) {
        return false;
      }
      bool rx_valid = (rx_pin == PIN_INVALID) || (std::find(rx_pins.begin(), rx_pins.end(), rx_pin) != rx_pins.end());
      bool tx_valid = (tx_pin == PIN_INVALID) || (std::find(tx_pins.begin(), tx_pins.end(), tx_pin) != tx_pins.end());
      return rx_valid && tx_valid;
    }
  };

  std::array<std::optional<UartInfo>, S_MAX_UARTS> uarts_ = {
#if LT_HW_UART0
#ifndef PINS_SERIAL0_TX
#define PINS_SERIAL0_TX \
  {}
#endif
#ifndef PINS_SERIAL0_RX
#define PINS_SERIAL0_RX \
  {}
#endif
      UartInfo{.hw_serial = Serial0, .tx_pins = PINS_SERIAL0_TX, .rx_pins = PINS_SERIAL0_RX},
#else
      std::nullopt,
#endif
#if LT_HW_UART1
#ifndef PINS_SERIAL1_TX
#define PINS_SERIAL1_TX \
  {}
#endif
#ifndef PINS_SERIAL1_RX
#define PINS_SERIAL1_RX \
  {}
#endif
      UartInfo{.hw_serial = Serial1, .tx_pins = PINS_SERIAL1_TX, .rx_pins = PINS_SERIAL1_RX},
#else
      std::nullopt,
#endif
#if LT_HW_UART2
#ifndef PINS_SERIAL2_TX
#define PINS_SERIAL2_TX \
  {}
#endif
#ifndef PINS_SERIAL2_RX
#define PINS_SERIAL2_RX \
  {}
#endif
      UartInfo{.hw_serial = Serial2, .tx_pins = PINS_SERIAL2_TX, .rx_pins = PINS_SERIAL2_RX}
#else
      std::nullopt
#endif
  };

  UartInfo *get_uart_by_number_(uint8_t uart_number);
  const UartInfo *get_uart_by_number_(uint8_t uart_number) const;
};

}  // namespace esphome::libretiny

#endif  // USE_LIBRETINY
