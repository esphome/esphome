#pragma once

#include <libretiny.h>
#include "esphome/core/helpers.h"

#include <optional>
#include <array>

namespace esphome::libretiny
{

struct UartManager
{
   const FixedVector<pin_size_t>& get_tx_pins_for_uart(const uint8_t uart_number) const
   {
      auto * uart = get_uart_by_number_(uart_number);
      if (!uart)
      {
         return get_empty_pins();
      }
      return uart->tx_pins;
   }

   const FixedVector<pin_size_t>& get_rx_pins_for_uart(const uint8_t uart_number) const
   {
      auto * uart = get_uart_by_number_(uart_number);
      if (!uart)
      {
         return get_empty_pins();
      }
      return uart->rx_pins;
   }

   HardwareSerial* get_hw_serial_by_number(const uint8_t uart_number)
   {
      auto * uart = get_uart_by_number_(uart_number);
      if (!uart)
      {
         return nullptr;
      }
      return &(uart->hw_serial);
   }

   FixedVector<uint8_t> get_available_uarts() const
   {
      return FixedVector<uint8_t>{
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

   uint8_t get_default_uart_number() const
   {
      return LT_UART_DEFAULT_SERIAL;
   }

   bool validate_pins(const uint8_t uart_number, const uint8_t rx_pin, const uint8_t tx_pin) const
   {
      auto * uart = get_uart_by_number_(uart_number);
      if (!uart)
      {
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
   bool init_uart(const uint8_t uart_number, uint32_t baud_rate, const uint16_t config)
   {
      auto * uart = get_uart_by_number_(uart_number);
      if (!uart)
      {
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
   bool init_uart(const uint8_t uart_number, uint32_t baud_rate, const uint16_t config, const uint8_t rx_pin, const uint8_t tx_pin)
   {
      auto * uart = get_uart_by_number_(uart_number);
      if (!uart)
      {
         return false;
      }

      if (!uart->validate_pins(rx_pin, tx_pin))
      {
         return false;
      }

      uart->hw_serial.begin(baud_rate, config, rx_pin, tx_pin);
      return true;
   }

   bool init_uart_for_logger(const uint8_t uart_number, uint32_t baud_rate)
   {
      auto * uart = get_uart_by_number_(uart_number);
      if (!uart)
      {
         return false;
      }

      uart->hw_serial.begin(baud_rate, SERIAL_8N1, uart->get_logger_rx_pin(), uart->get_logger_tx_pin());
      return true;
   }

   void deinit_uart(const uint8_t uart_number)
   {
      auto * uart = get_uart_by_number_(uart_number);
      if (!uart)
      {
         return;
      }

      uart->hw_serial.end();
   }

private:

   static const FixedVector<pin_size_t>& get_empty_pins()
   {
      static const FixedVector<pin_size_t> s_empty_pins;
      return s_empty_pins;
   }

   static constexpr size_t s_max_uarts = 3;

   struct UartInfo
   {
      ::SerialClass& hw_serial;
      FixedVector<pin_size_t> tx_pins;
      FixedVector<pin_size_t> rx_pins;

      uint8_t get_logger_rx_pin() const
      {
         if (rx_pins.size() > 0)
         {
            return rx_pins[0];
         }
         return PIN_INVALID;
      };

      uint8_t get_logger_tx_pin() const
      {
         if (tx_pins.size() > 0)
         {
            return tx_pins[0];
         }
         return PIN_INVALID;
      };

      bool validate_pins(const uint8_t rx_pin, const uint8_t tx_pin) const
      {
         if ((rx_pin == PIN_INVALID) && (tx_pin == PIN_INVALID))
         {
            return false;
         }
         bool rx_valid = (rx_pin == PIN_INVALID) || (std::find(rx_pins.begin(), rx_pins.end(), rx_pin) != rx_pins.end());
         bool tx_valid = (tx_pin == PIN_INVALID) || (std::find(tx_pins.begin(), tx_pins.end(), tx_pin) != tx_pins.end());
         return rx_valid && tx_valid;
      }
   };

   // note: cannot use FixedVector for this, as it only works with trivially constructible types
   //       cannot use array, as it's difficult to calculate the total size based on defines from boards
   std::array<std::optional<UartInfo>, s_max_uarts> uarts_ = {
#if LT_HW_UART0
   #ifndef PINS_SERIAL0_TX
   #define PINS_SERIAL0_TX {}
   #endif
   #ifndef PINS_SERIAL0_RX
   #define PINS_SERIAL0_RX {}
   #endif
         UartInfo{.hw_serial = Serial0, .tx_pins = PINS_SERIAL0_TX, .rx_pins = PINS_SERIAL0_RX},
#else
         std::nullopt,
#endif
#if LT_HW_UART1
   #ifndef PINS_SERIAL1_TX
   #define PINS_SERIAL1_TX {}
   #endif
   #ifndef PINS_SERIAL1_RX
   #define PINS_SERIAL1_RX {}
#endif
         UartInfo{.hw_serial = Serial1, .tx_pins = PINS_SERIAL1_TX, .rx_pins = PINS_SERIAL1_RX},
#else
         std::nullopt,
#endif
#if LT_HW_UART2
   #ifndef PINS_SERIAL2_TX
   #define PINS_SERIAL2_TX {}
   #endif
   #ifndef PINS_SERIAL2_RX
   #define PINS_SERIAL2_RX {}
#endif
         UartInfo{.hw_serial = Serial2, .tx_pins = PINS_SERIAL2_TX, .rx_pins = PINS_SERIAL2_RX}
#else
         std::nullopt
#endif
   };

   UartInfo* get_uart_by_number_(const uint8_t uart_number)
   {
      if ((uart_number >= s_max_uarts) || (!uarts_[uart_number].has_value()))
      {
         return nullptr;
      }
      return &uarts_[uart_number].value();
   }

   const UartInfo* get_uart_by_number_(const uint8_t uart_number) const
   {
      if ((uart_number >= s_max_uarts) || (!uarts_[uart_number].has_value()))
      {
         return nullptr;
      }
      return &uarts_[uart_number].value();
   }
};

}
