#pragma once

#include "esphome/components/improv_base/improv_base.h"
#include "esphome/components/wifi/wifi_component.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#ifdef USE_WIFI
#include <improv.h>
#include <vector>

#ifdef USE_ESP32
#include <driver/uart.h>
#if defined(USE_ESP32_VARIANT_ESP32C3) || defined(USE_ESP32_VARIANT_ESP32C6) || defined(USE_ESP32_VARIANT_ESP32C61) || \
    defined(USE_ESP32_VARIANT_ESP32H2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include <driver/usb_serial_jtag.h>
#include <hal/usb_serial_jtag_ll.h>
#endif
#if defined(USE_ESP32_VARIANT_ESP32S2) || defined(USE_ESP32_VARIANT_ESP32S3)
#include <esp_private/usb_console.h>
#endif
#elif defined(USE_ARDUINO)
#include <HardwareSerial.h>
#endif

namespace esphome {
namespace improv_serial {

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

class ImprovSerialComponent : public Component, public improv_base::ImprovBase {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;

  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }

 protected:
  bool parse_improv_serial_byte_(uint8_t byte);
  bool parse_improv_payload_(improv::ImprovCommand &command);

  void set_state_(improv::State state);
  void set_error_(improv::Error error);
  void send_response_(std::vector<uint8_t> &response);
  void on_wifi_connect_timeout_();

  std::vector<uint8_t> build_rpc_settings_response_(improv::Command command);
  std::vector<uint8_t> build_version_info_();

  optional<uint8_t> read_byte_();
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

#ifdef USE_ESP32
  uart_port_t uart_num_;
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

}  // namespace improv_serial
}  // namespace esphome
#endif
