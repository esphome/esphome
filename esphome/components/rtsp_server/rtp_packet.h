#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome::rtsp_server {

// Interleaved RTP over RTSP/TCP (RFC 2326). Packetizers supply the payload type,
// marker bit, and payload length; sessions retain sequence/timestamp ownership.
static constexpr size_t RTP_INTERLEAVED_HEADER_SIZE = 4;
static constexpr size_t RTP_FIXED_HEADER_SIZE = 12;
static constexpr size_t RTP_TCP_HEADER_SIZE = RTP_INTERLEAVED_HEADER_SIZE + RTP_FIXED_HEADER_SIZE;

inline size_t write_interleaved_rtp_header(uint8_t *buffer, uint8_t channel, uint8_t payload_type, bool marker,
                                           size_t payload_length, uint16_t sequence, uint32_t timestamp,
                                           uint32_t ssrc) {
  if (buffer == nullptr || payload_type > 127 || payload_length > 65535 - RTP_FIXED_HEADER_SIZE)
    return 0;
  const uint16_t rtp_length = static_cast<uint16_t>(RTP_FIXED_HEADER_SIZE + payload_length);
  buffer[0] = '$';
  buffer[1] = channel;
  buffer[2] = static_cast<uint8_t>(rtp_length >> 8);
  buffer[3] = static_cast<uint8_t>(rtp_length);
  buffer[4] = 0x80;
  buffer[5] = static_cast<uint8_t>((marker ? 0x80U : 0U) | payload_type);
  buffer[6] = static_cast<uint8_t>(sequence >> 8);
  buffer[7] = static_cast<uint8_t>(sequence);
  buffer[8] = static_cast<uint8_t>(timestamp >> 24);
  buffer[9] = static_cast<uint8_t>(timestamp >> 16);
  buffer[10] = static_cast<uint8_t>(timestamp >> 8);
  buffer[11] = static_cast<uint8_t>(timestamp);
  buffer[12] = static_cast<uint8_t>(ssrc >> 24);
  buffer[13] = static_cast<uint8_t>(ssrc >> 16);
  buffer[14] = static_cast<uint8_t>(ssrc >> 8);
  buffer[15] = static_cast<uint8_t>(ssrc);
  return RTP_TCP_HEADER_SIZE;
}

}  // namespace esphome::rtsp_server
