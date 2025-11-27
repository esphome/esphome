#pragma once

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/core/component.h"
#include "esphome/core/helpers.h"
#include "esphome/components/uart/uart.h"

#include <array>

namespace esphome::zwave_proxy {

static constexpr size_t MAX_ZWAVE_FRAME_SIZE = 257;  // Maximum Z-Wave frame size

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
  bool set_home_id(const uint8_t *new_home_id);  // Store a new home ID. Returns true if it changed.

  void send_frame(const uint8_t *data, size_t length);

 protected:
  void send_homeid_changed_msg_(api::APIConnection *conn = nullptr);
  void send_simple_command_(uint8_t command_id);
  bool parse_byte_(uint8_t byte);  // Returns true if frame parsing was completed (a frame is ready in the buffer)
  void parse_start_(uint8_t byte);
  bool response_handler_();
  void process_uart_();  // Process all available UART data

  // Pre-allocated message - always ready to send
  api::ZWaveProxyFrame outgoing_proto_msg_;
  std::array<uint8_t, MAX_ZWAVE_FRAME_SIZE> buffer_;  // Fixed buffer for incoming data
  std::array<uint8_t, 4> home_id_{0, 0, 0, 0};        // Fixed buffer for home ID

  // Pointers and 32-bit values (aligned together)
  api::APIConnection *api_connection_{nullptr};  // Current subscribed client
  uint32_t setup_time_{0};                       // Time when setup() was called

  // 8-bit values (grouped together to minimize padding)
  uint8_t buffer_index_{0};     // Index for populating the data buffer
  uint8_t end_frame_after_{0};  // Payload reception ends after this index
  uint8_t last_response_{0};    // Last response type sent
  ZWaveParsingState parsing_state_{ZWAVE_PARSING_STATE_WAIT_START};
  bool in_bootloader_{false};  // True if the device is detected to be in bootloader mode
  bool home_id_ready_{false};  // True when home ID has been received from Z-Wave module
};

extern ZWaveProxy *global_zwave_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace esphome::zwave_proxy
