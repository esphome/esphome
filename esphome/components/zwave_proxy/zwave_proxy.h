#pragma once

#include "esphome/components/api/api_connection.h"
#include "esphome/components/api/api_pb2.h"
#include "esphome/core/component.h"
#include "esphome/components/uart/uart.h"

namespace esphome {
namespace zwave_proxy {

enum ZWaveResponseTypes : uint8_t {
  ZWAVE_FRAME_TYPE_ACK = 0x06,
  ZWAVE_FRAME_TYPE_CAN = 0x18,
  ZWAVE_FRAME_TYPE_NAK = 0x15,
  ZWAVE_FRAME_TYPE_START = 0x01,
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
};

enum ZWaveProxyFeature : uint32_t {
  FEATURE_ZWAVE_PROXY_ENABLED = 1 << 0,
};

class ZWaveProxy : public uart::UARTDevice, public Component {
 public:
  ZWaveProxy();

  void loop() override;
  void dump_config() override;

  void subscribe_api_connection(api::APIConnection *api_connection, uint32_t flags);
  void unsubscribe_api_connection(api::APIConnection *api_connection);
  api::APIConnection *get_api_connection() { return this->api_connection_; }

  uint32_t get_feature_flags() const { return ZWaveProxyFeature::FEATURE_ZWAVE_PROXY_ENABLED; }

  void send_frame(const std::string &data);
  void send_frame(const std::vector<uint8_t> &data);

 protected:
  bool parse_byte_(uint8_t byte);  // Returns true if frame parsing was completed (a frame is ready in the buffer)
  void parse_start_(uint8_t byte);
  bool response_handler_();

  api::APIConnection *api_connection_{nullptr};  // Current subscribed client

  uint8_t buffer_[257];         // Fixed buffer for incoming data: max length = 255 + 2 (start of frame and checksum)
  uint8_t buffer_index_{0};     // Index for populating the data buffer
  uint8_t checksum_{0};         // Checksum of the frame being parsed
  uint8_t end_frame_after_{0};  // Payload reception ends after this index
  ZWaveParsingState parsing_state_{ZWAVE_PARSING_STATE_WAIT_START};

  // Pre-allocated message - always ready to send
  api::ZWaveProxyFrameFromDevice outgoing_proto_msg_;
};

extern ZWaveProxy *global_zwave_proxy;  // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)

}  // namespace zwave_proxy
}  // namespace esphome
