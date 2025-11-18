#pragma once
#include "api_frame_helper.h"
#ifdef USE_API
#ifdef USE_API_NOISE
#include "noise/protocol.h"
#include "api_noise_context.h"

namespace esphome::api {

class APINoiseFrameHelper final : public APIFrameHelper {
 public:
  APINoiseFrameHelper(std::unique_ptr<socket::Socket> socket, APINoiseContext &ctx, const ClientInfo *client_info)
      : APIFrameHelper(std::move(socket), client_info), ctx_(ctx) {
    // Noise header structure:
    // Pos 0: indicator (0x01)
    // Pos 1-2: encrypted payload size (16-bit big-endian)
    // Pos 3-6: encrypted type (16-bit) + data_len (16-bit)
    // Pos 7+: actual payload data
    frame_header_padding_ = 7;
  }
  ~APINoiseFrameHelper() override;
  APIError init() override;
  APIError loop() override;
  APIError read_packet(ReadPacketBuffer *buffer) override;
  APIError write_protobuf_packet(uint8_t type, ProtoWriteBuffer buffer) override;
  APIError write_protobuf_packets(ProtoWriteBuffer buffer, std::span<const PacketInfo> packets) override;

 protected:
  APIError state_action_();
  APIError try_read_frame_();
  APIError write_frame_(const uint8_t *data, uint16_t len);
  APIError init_handshake_();
  APIError check_handshake_finished_();
  void send_explicit_handshake_reject_(const LogString *reason);
  APIError handle_handshake_frame_error_(APIError aerr);
  APIError handle_noise_error_(int err, const LogString *func_name, APIError api_err);

  // Pointers first (4 bytes each)
  NoiseHandshakeState *handshake_{nullptr};
  NoiseCipherState *send_cipher_{nullptr};
  NoiseCipherState *recv_cipher_{nullptr};

  // Reference to noise context (4 bytes on 32-bit)
  APINoiseContext &ctx_;

  // Vector (12 bytes on 32-bit)
  std::vector<uint8_t> prologue_;

  // NoiseProtocolId (size depends on implementation)
  NoiseProtocolId nid_;

  // Group small types together
  // Fixed-size header buffer for noise protocol:
  // 1 byte for indicator + 2 bytes for message size (16-bit value, not varint)
  // Note: Maximum message size is UINT16_MAX (65535), with a limit of 128 bytes during handshake phase
  uint8_t rx_header_buf_[3];
  uint8_t rx_header_buf_len_ = 0;
  // 4 bytes total, no padding
};

}  // namespace esphome::api
#endif  // USE_API_NOISE
#endif  // USE_API
