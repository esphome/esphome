#pragma once
#include "api_frame_helper.h"
#ifdef USE_API
#ifdef USE_API_PLAINTEXT

namespace esphome::api {

class APIPlaintextFrameHelper final : public APIFrameHelper {
 public:
  APIPlaintextFrameHelper(std::unique_ptr<socket::Socket> socket, const ClientInfo *client_info)
      : APIFrameHelper(std::move(socket), client_info) {
    // Plaintext header structure (worst case):
    // Pos 0: indicator (0x00)
    // Pos 1-3: payload size varint (up to 3 bytes)
    // Pos 4-5: message type varint (up to 2 bytes)
    // Pos 6+: actual payload data
    frame_header_padding_ = 6;
  }
  ~APIPlaintextFrameHelper() override = default;
  APIError init() override;
  APIError loop() override;
  APIError read_packet(ReadPacketBuffer *buffer) override;
  APIError write_protobuf_packet(uint8_t type, ProtoWriteBuffer buffer) override;
  APIError write_protobuf_packets(ProtoWriteBuffer buffer, std::span<const PacketInfo> packets) override;

 protected:
  APIError try_read_frame_();

  // Group 2-byte aligned types
  uint16_t rx_header_parsed_type_ = 0;
  uint16_t rx_header_parsed_len_ = 0;

  // Group 1-byte types together
  // Fixed-size header buffer for plaintext protocol:
  // We now store the indicator byte + the two varints.
  // To match noise protocol's maximum message size (UINT16_MAX = 65535), we need:
  // 1 byte for indicator + 3 bytes for message size varint (supports up to 2097151) + 2 bytes for message type varint
  //
  // While varints could theoretically be up to 10 bytes each for 64-bit values,
  // attempting to process messages with headers that large would likely crash the
  // ESP32 due to memory constraints.
  uint8_t rx_header_buf_[6];  // 1 byte indicator + 5 bytes for varints (3 for size + 2 for type)
  uint8_t rx_header_buf_pos_ = 0;
  bool rx_header_parsed_ = false;
  // 8 bytes total, no padding needed
};

}  // namespace esphome::api
#endif  // USE_API_PLAINTEXT
#endif  // USE_API
