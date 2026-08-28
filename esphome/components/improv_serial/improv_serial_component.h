#pragma once

#include "esphome/components/improv_base/improv_base.h"
#include "esphome/components/logger/logger.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#ifdef USE_WIFI
#include <improv.h>
#include <span>
#include <vector>

#ifdef USE_IMPROV_SERIAL_UART
#include "esphome/components/uart/uart_component.h"
#elif defined(USE_ESP32)
#include <driver/uart.h>
#ifdef USE_LOGGER_USB_SERIAL_JTAG
#include <driver/usb_serial_jtag.h>
#include <hal/usb_serial_jtag_ll.h>
#endif
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include <esp_private/usb_console.h>
#endif
#elif defined(USE_ARDUINO)
#include <HardwareSerial.h>
#endif

namespace esphome::improv_serial {

// TX buffer layout constants
static constexpr uint8_t TX_HEADER_SIZE = 6;  // Bytes 0-5 = "IMPROV"
static constexpr uint8_t TX_VERSION_IDX = 6;
static constexpr uint8_t TX_TYPE_IDX = 7;
static constexpr uint8_t TX_LENGTH_IDX = 8;
static constexpr uint8_t TX_DATA_IDX = 9;  // For state/error messages only
static constexpr uint8_t TX_CHECKSUM_IDX = 10;
static constexpr uint8_t TX_NEWLINE_IDX = 11;
static constexpr uint8_t TX_BUFFER_SIZE = 12;

enum ImprovSerialType : uint8_t {
  TYPE_CURRENT_STATE = 0x01,
  TYPE_ERROR_STATE = 0x02,
  TYPE_RPC = 0x03,
  TYPE_RPC_RESPONSE = 0x04
};

static const uint16_t IMPROV_SERIAL_TIMEOUT = 100;
static const uint8_t IMPROV_SERIAL_VERSION = 1;

// The serial frame length field is one byte
static constexpr size_t MAX_SERIAL_RESPONSE = 255;
// command + data length + trailing byte
static constexpr size_t RPC_RESPONSE_OVERHEAD = 3;
static constexpr size_t MAX_SERIAL_PAYLOAD = MAX_SERIAL_RESPONSE - RPC_RESPONSE_OVERHEAD;
#ifdef USE_WEBSERVER
// length byte + "http://" + IPv4 + ":" + port
static constexpr size_t WEBSERVER_URL_RESERVE = 1 + 7 + 15 + 1 + 5;
#else
static constexpr size_t WEBSERVER_URL_RESERVE = 0;
#endif
// Entry budget minus its own length byte
static constexpr size_t MAX_NEXT_URL_LEN = MAX_SERIAL_PAYLOAD - WEBSERVER_URL_RESERVE - 1;

static_assert(MAX_SERIAL_RESPONSE <= improv::RPC_RESPONSE_MAX_SIZE, "builder buffer too small for the frame");

class ImprovSerialComponent final : public Component, public improv_base::ImprovBase {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

#ifdef USE_IMPROV_SERIAL_UART
  void set_uart(uart::UARTComponent *uart) { this->uart_ = uart; }
#endif

 protected:
  bool parse_improv_serial_byte_(uint8_t byte);
  bool parse_improv_payload_(improv::ImprovCommand &command);

  void set_state_(improv::State state);
  void send_current_state_(improv::State state);
  void set_error_(improv::Error error);
  void send_response_(std::span<const uint8_t> response);
  void on_wifi_connect_timeout_();

  void send_settings_response_(improv::Command command);
  void send_version_info_();

  ESPHOME_ALWAYS_INLINE optional<uint8_t> read_byte_() {
    optional<uint8_t> byte;
    uint8_t data = 0;
#ifdef USE_IMPROV_SERIAL_UART
    if (this->uart_->available() && this->uart_->read_byte(&data)) {
      byte = data;
    }
#elif defined(USE_ESP32)
    switch (this->uart_selection_) {
      case logger::UART_SELECTION_UART0:
      case logger::UART_SELECTION_UART1:
#if defined(USE_ESP32_VARIANT_ESP32)
      case logger::UART_SELECTION_UART2:
#endif
        if (this->uart_num_ >= 0) {
          size_t available;
          uart_get_buffered_data_len(this->uart_num_, &available);
          if (available) {
            uart_read_bytes(this->uart_num_, &data, 1, 0);
            byte = data;
          }
        }
        break;
#if defined(USE_LOGGER_USB_CDC) && defined(CONFIG_ESP_CONSOLE_USB_CDC)
      case logger::UART_SELECTION_USB_CDC:
        if (esp_usb_console_available_for_read()) {
          esp_usb_console_read_buf((char *) &data, 1);
          byte = data;
        }
        break;
#endif
#ifdef USE_LOGGER_USB_SERIAL_JTAG
      case logger::UART_SELECTION_USB_SERIAL_JTAG: {
        if (usb_serial_jtag_read_bytes((char *) &data, 1, 0)) {
          byte = data;
        }
        break;
      }
#endif
      default:
        break;
    }
#elif defined(USE_ARDUINO)
    if (this->hw_serial_->available()) {
      this->hw_serial_->readBytes(&data, 1);
      byte = data;
    }
#endif
    return byte;
  }
  void write_data_(const uint8_t *data = nullptr, size_t size = 0);

  uint8_t tx_header_[TX_BUFFER_SIZE] = {
      'I',                    // 0: Header
      'M',                    // 1: Header
      'P',                    // 2: Header
      'R',                    // 3: Header
      'O',                    // 4: Header
      'V',                    // 5: Header
      IMPROV_SERIAL_VERSION,  // 6: Version
      0,                      // 7: ImprovSerialType
      0,                      // 8: Length
      0,                      // 9...X: Data (here, one byte reserved for state/error)
      0,                      // X + 10: Checksum
      '\n',
  };

#ifdef USE_IMPROV_SERIAL_UART
  uart::UARTComponent *uart_{nullptr};
#elif defined(USE_ESP32)
  uart_port_t uart_num_;
  logger::UARTSelection uart_selection_{logger::UART_SELECTION_UART0};
#elif defined(USE_ARDUINO)
  Stream *hw_serial_{nullptr};
#endif

  std::vector<uint8_t> rx_buffer_;
  uint32_t last_read_byte_{0};
  wifi::WiFiAP connecting_sta_;
  improv::State state_{improv::STATE_AUTHORIZED};
};

extern ImprovSerialComponent
    *global_improv_serial_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::improv_serial

#endif
