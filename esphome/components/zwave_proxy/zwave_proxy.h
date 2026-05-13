#pragma once

#include "esphome/core/defines.h"
#ifdef USE_API

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#include <array>

namespace esphome::zwave_proxy {

static constexpr size_t MAX_ZWAVE_FRAME_SIZE = 257;  // Maximum Z-Wave frame size
static constexpr size_t ZWAVE_HOME_ID_SIZE = 4;      // Z-Wave Home ID size in bytes

enum ZWaveResponseTypes : uint8_t {
  ZWAVE_FRAME_TYPE_ACK = 0x06,
  ZWAVE_FRAME_TYPE_CAN = 0x18,
  ZWAVE_FRAME_TYPE_NAK = 0x15,
  ZWAVE_FRAME_TYPE_START = 0x01,
  ZWAVE_FRAME_TYPE_BL_MENU = 0x0D,
  ZWAVE_FRAME_TYPE_BL_BEGIN_UPLOAD = 0x43,
};

enum ZWaveParsingState : uint8_t {
  ZWAVE_PARSING_STATE_WAIT_START,
  ZWAVE_PARSING_STATE_WAIT_LENGTH,
  ZWAVE_PARSING_STATE_WAIT_TYPE,
  ZWAVE_PARSING_STATE_WAIT_COMMAND_ID,
  ZWAVE_PARSING_STATE_WAIT_PAYLOAD,
  ZWAVE_PARSING_STATE_WAIT_CHECKSUM,
  ZWAVE_PARSING_STATE_SEND_ACK,
  ZWAVE_PARSING_STATE_SEND_CAN,
  ZWAVE_PARSING_STATE_SEND_NAK,
  ZWAVE_PARSING_STATE_READ_BL_MENU,
};

// response_handler_()'s inline fast-path relies on SEND_ACK/CAN/NAK being contiguous in this
// enum so a single range check (state - SEND_ACK < 3) is equivalent to three equality checks.
static_assert(ZWAVE_PARSING_STATE_SEND_CAN == ZWAVE_PARSING_STATE_SEND_ACK + 1,
              "SEND_CAN must immediately follow SEND_ACK for response_handler_ fast-path");
static_assert(ZWAVE_PARSING_STATE_SEND_NAK == ZWAVE_PARSING_STATE_SEND_ACK + 2,
              "SEND_NAK must immediately follow SEND_CAN for response_handler_ fast-path");

enum ZWaveProxyFeature : uint32_t {
  FEATURE_ZWAVE_PROXY_ENABLED = 1 << 0,
};

class ZWaveProxy : public uart::UARTDevice, public Component {
 public:
  ZWaveProxy();

  void setup() override;
  void loop() override;
  void dump_config() override;
  float get_setup_priority() const override;
  bool can_proceed() override;

  void api_connection_authenticated(api::APIConnection *conn);
  void zwave_proxy_request(api::APIConnection *api_connection, api::enums::ZWaveProxyRequestType type);
  api::APIConnection *get_api_connection() { return this->api_connection_; }

  uint32_t get_feature_flags() const { return ZWaveProxyFeature::FEATURE_ZWAVE_PROXY_ENABLED; }
  uint32_t get_home_id() {
    return encode_uint32(this->home_id_[0], this->home_id_[1], this->home_id_[2], this->home_id_[3]);
  }

  void send_frame(const uint8_t *data, size_t length);

 protected:
  bool set_home_id_(const uint8_t *new_home_id);  // Store a new home ID. Returns true if it changed.
  void clear_home_id_();                          // Clear home ID and notify API clients
  void on_connection_changed_(bool connected);    // Handle modem connect/disconnect transitions
  void retry_home_id_query_();                    // Retry home ID query after reconnect
  void send_homeid_changed_msg_(api::APIConnection *conn = nullptr);
  void send_simple_command_(uint8_t command_id);
  bool parse_byte_(uint8_t byte);  // Returns true if frame parsing was completed (a frame is ready in the buffer)
  void parse_start_(uint8_t byte);
  // Inline fast-path: most calls happen with parsing_state_ outside the SEND_* range, so skip the
  // out-of-line call entirely in the hot path (e.g. every loop() tick) and only pay for the real
  // work when a response is actually pending. ESPHOME_ALWAYS_INLINE is required because with -Os
  // gcc otherwise clones the wrapper into a shared $isra$ outline and keeps the call8.
  ESPHOME_ALWAYS_INLINE bool response_handler_() {
    if (this->parsing_state_ < ZWAVE_PARSING_STATE_SEND_ACK || this->parsing_state_ > ZWAVE_PARSING_STATE_SEND_NAK) {
      return false;
    }
    return this->response_handler_slow_();
  }
  bool response_handler_slow_();
  // Inline fast-path: UART::available() is cheap (ring-buffer head/tail compare on most backends).
  // On an idle loop tick we want to skip the call to process_uart_ entirely. When bytes are
  // pending we fall into the slow path, which drains the UART with a do/while so available() is
  // only checked once per byte — no redundant re-check on entry.
  ESPHOME_ALWAYS_INLINE void process_uart_() {
    if (!this->available()) {
      return;
    }
    this->process_uart_slow_();
  }
  // Precondition: caller must guarantee available() > 0 before invoking (see inline
  // process_uart_ above). The slow path uses do/while and would otherwise set a spurious UART
  // warning on entry if called with no bytes pending.
  void process_uart_slow_();

  // Pre-allocated message - always ready to send
  api::ZWaveProxyFrame outgoing_proto_msg_;
  std::array<uint8_t, MAX_ZWAVE_FRAME_SIZE> buffer_;   // Fixed buffer for incoming data
  std::array<uint8_t, ZWAVE_HOME_ID_SIZE> home_id_{};  // Fixed buffer for home ID

  // Pointers and 32-bit values (aligned together)
  api::APIConnection *api_connection_{nullptr};  // Current subscribed client
  uint32_t setup_time_{0};                       // Time when setup() was called
  uint32_t reconnect_time_{0};                   // Timestamp of reconnect detection (0 = no pending query)

  // Small values (grouped by size to minimize padding)
  uint16_t buffer_index_{0};     // Index for populating the data buffer
  uint16_t end_frame_after_{0};  // Payload reception ends after this index
  uint8_t last_response_{0};     // Last response type sent
  uint8_t query_retries_{0};     // Number of home ID query attempts after reconnect
  ZWaveParsingState parsing_state_{ZWAVE_PARSING_STATE_WAIT_START};
  bool in_bootloader_{false};  // True if the device is detected to be in bootloader mode
  bool home_id_ready_{false};  // True when home ID has been received from Z-Wave module
  bool was_connected_{false};  // Previous UART connection state for edge detection
};

extern ZWaveProxy *global_zwave_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::zwave_proxy

#endif  // USE_API
