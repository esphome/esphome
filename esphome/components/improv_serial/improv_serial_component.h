#pragma once

#include "esphome/components/improv_base/improv_base.h"
#include "esphome/components/logger/logger.h"
#include "esphome/components/network/util.h"
#include "esphome/core/component.h"
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#ifdef USE_IMPROV_SERIAL
#include <improv.h>
#include <span>
#include <vector>

#ifdef USE_WIFI
#include <memory>
#include "esphome/components/wifi/wifi_component.h"
#endif

#ifdef USE_IMPROV_SERIAL_UART
#include "esphome/components/uart/uart_component.h"
#endif

namespace esphome::improv_serial {

enum ImprovSerialType : uint8_t {
  TYPE_CURRENT_STATE = 0x01,
  TYPE_ERROR_STATE = 0x02,
  TYPE_RPC = 0x03,
  TYPE_RPC_RESPONSE = 0x04
};

static constexpr uint16_t IMPROV_SERIAL_TIMEOUT = 100;
static constexpr uint8_t IMPROV_SERIAL_VERSION = 1;
// "IMPROV", version, type, length
static constexpr size_t FRAME_HEADER_SIZE = 9;

#ifdef USE_WIFI
// Wi-Fi connect failure timers: a fresh provision reports at 30 s (stock behavior), while
// switching networks on an already-connected device (disconnect + reconnect) can legitimately
// take longer; 90 s matches improv_ble's default wifi_timeout.
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
static constexpr uint32_t WIFI_SWITCH_TIMEOUT_MS = 90000;
#endif

// The serial frame length field is one byte
static constexpr size_t MAX_SERIAL_RESPONSE = 255;
// command + data length + trailing byte
static constexpr size_t RPC_RESPONSE_OVERHEAD = 3;
static constexpr size_t MAX_SERIAL_PAYLOAD = MAX_SERIAL_RESPONSE - RPC_RESPONSE_OVERHEAD;
#ifdef USE_WEBSERVER
// length byte + "http://" + IPv4 + ":" + port. Reserves the first URL only; a device with
// several interfaces online adds the rest best-effort and warns if one no longer fits.
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
  /// Free the frame buffer; a session holds it only while a frame is arriving.
  void reset_rx_buffer_() { std::vector<uint8_t>().swap(this->rx_buffer_); }

  void set_state_(improv::State state);
  void send_state_byte_(ImprovSerialType type, uint8_t value);
  void send_response_(std::span<const uint8_t> response);
#ifdef USE_WIFI
  void on_wifi_connect_timeout_();
#endif

#ifdef USE_WEBSERVER
  /// Append one web server URL per interface that has a usable IPv4. With wifi_first the Wi-Fi
  /// URL leads, for responses to Wi-Fi provisioning; otherwise interfaces go in priority order.
  void add_webserver_urls_(improv::RpcResponseBuilder &builder, [[maybe_unused]] bool wifi_first);
#endif
  void send_settings_response_(improv::Command command);
  void send_version_info_();

  ESPHOME_ALWAYS_INLINE bool read_byte_(uint8_t &byte) {
#ifdef USE_IMPROV_SERIAL_UART
    return this->uart_->available() && this->uart_->read_byte(&byte);
#else
    return logger::global_logger->console_read(byte);
#endif
  }
  ESPHOME_ALWAYS_INLINE void write_(const uint8_t *data, size_t len) {
#ifdef USE_IMPROV_SERIAL_UART
    this->uart_->write_array(data, len);
#else
    logger::global_logger->console_write(data, len);
#endif
  }

#ifdef USE_IMPROV_SERIAL_UART
  uart::UARTComponent *uart_{nullptr};
#endif

  std::vector<uint8_t> rx_buffer_;
  uint32_t last_read_byte_{0};
#ifdef USE_WIFI
  // Only held while provisioning
  std::unique_ptr<wifi::WiFiAP> connecting_sta_;
#endif
  improv::State state_{improv::STATE_AUTHORIZED};
};

extern ImprovSerialComponent
    *global_improv_serial_component;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::improv_serial

#endif
