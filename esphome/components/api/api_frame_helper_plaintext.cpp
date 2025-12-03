#include "api_frame_helper_plaintext.h"
#ifdef USE_API
#ifdef USE_API_PLAINTEXT
#include "api_connection.h"  // For ClientInfo struct
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "proto.h"
#include <cstring>
#include <cinttypes>

#ifdef USE_ESP8266
#include <pgmspace.h>
#endif

namespace esphome::api {

static const char *const TAG = "api.plaintext";

#define HELPER_LOG(msg, ...) \
  ESP_LOGVV(TAG, "%s (%s): " msg, this->client_info_->name.c_str(), this->client_info_->peername.c_str(), ##__VA_ARGS__)

#ifdef HELPER_LOG_PACKETS
#define LOG_PACKET_RECEIVED(buffer) ESP_LOGVV(TAG, "Received frame: %s", format_hex_pretty(buffer).c_str())
#define LOG_PACKET_SENDING(data, len) ESP_LOGVV(TAG, "Sending raw: %s", format_hex_pretty(data, len).c_str())
#else
#define LOG_PACKET_RECEIVED(buffer) ((void) 0)
#define LOG_PACKET_SENDING(data, len) ((void) 0)
#endif

/// Initialize the frame helper, returns OK if successful.
APIError APIPlaintextFrameHelper::init() {
  APIError err = init_common_();
  if (err != APIError::OK) {
    return err;
  }

  state_ = State::DATA;
  return APIError::OK;
}
APIError APIPlaintextFrameHelper::loop() {
  if (state_ != State::DATA) {
    return APIError::BAD_STATE;
  }
  // Use base class implementation for buffer sending
  return APIFrameHelper::loop();
}

/** Read a packet into the rx_buf_.
 *
 * @return See APIError
 *
 * error API_ERROR_BAD_INDICATOR: Bad indicator byte at start of frame.
 */
APIError APIPlaintextFrameHelper::try_read_frame_() {
  // read header
  while (!rx_header_parsed_) {
    // Now that we know when the socket is ready, we can read up to 3 bytes
    // into the rx_header_buf_ before we have to switch back to reading
    // one byte at a time to ensure we don't read past the message and
    // into the next one.

    // Read directly into rx_header_buf_ at the current position
    // Try to get to at least 3 bytes total (indicator + 2 varint bytes), then read one byte at a time
    ssize_t received =
        this->socket_->read(&rx_header_buf_[rx_header_buf_pos_], rx_header_buf_pos_ < 3 ? 3 - rx_header_buf_pos_ : 1);
    APIError err = handle_socket_read_result_(received);
    if (err != APIError::OK) {
      return err;
    }

    // If this was the first read, validate the indicator byte
    if (rx_header_buf_pos_ == 0 && received > 0) {
      if (rx_header_buf_[0] != 0x00) {
        state_ = State::FAILED;
        HELPER_LOG("Bad indicator byte %u", rx_header_buf_[0]);
        return APIError::BAD_INDICATOR;
      }
    }

    rx_header_buf_pos_ += received;

    // Check for buffer overflow
    if (rx_header_buf_pos_ >= sizeof(rx_header_buf_)) {
      state_ = State::FAILED;
      HELPER_LOG("Header buffer overflow");
      return APIError::BAD_DATA_PACKET;
    }

    // Need at least 3 bytes total (indicator + 2 varint bytes) before trying to parse
    if (rx_header_buf_pos_ < 3) {
      continue;
    }

    // At this point, we have at least 3 bytes total:
    //   - Validated indicator byte (0x00) stored at position 0
    //   - At least 2 bytes in the buffer for the varints
    // Buffer layout:
    //   [0]: indicator byte (0x00)
    //   [1-3]: Message size varint (variable length)
    //     - 2 bytes would only allow up to 16383, which is less than noise's UINT16_MAX (65535)
    //     - 3 bytes allows up to 2097151, ensuring we support at least as much as noise
    //   [2-5]: Message type varint (variable length)
    // We now attempt to parse both varints. If either is incomplete,
    // we'll continue reading more bytes.

    // Skip indicator byte at position 0
    uint8_t varint_pos = 1;
    uint32_t consumed = 0;

    auto msg_size_varint = ProtoVarInt::parse(&rx_header_buf_[varint_pos], rx_header_buf_pos_ - varint_pos, &consumed);
    if (!msg_size_varint.has_value()) {
      // not enough data there yet
      continue;
    }

    if (msg_size_varint->as_uint32() > MAX_MESSAGE_SIZE) {
      state_ = State::FAILED;
      HELPER_LOG("Bad packet: message size %" PRIu32 " exceeds maximum %u", msg_size_varint->as_uint32(),
                 MAX_MESSAGE_SIZE);
      return APIError::BAD_DATA_PACKET;
    }
    rx_header_parsed_len_ = msg_size_varint->as_uint16();

    // Move to next varint position
    varint_pos += consumed;

    auto msg_type_varint = ProtoVarInt::parse(&rx_header_buf_[varint_pos], rx_header_buf_pos_ - varint_pos, &consumed);
    if (!msg_type_varint.has_value()) {
      // not enough data there yet
      continue;
    }
    if (msg_type_varint->as_uint32() > std::numeric_limits<uint16_t>::max()) {
      state_ = State::FAILED;
      HELPER_LOG("Bad packet: message type %" PRIu32 " exceeds maximum %u", msg_type_varint->as_uint32(),
                 std::numeric_limits<uint16_t>::max());
      return APIError::BAD_DATA_PACKET;
    }
    rx_header_parsed_type_ = msg_type_varint->as_uint16();
    rx_header_parsed_ = true;
  }
  // header reading done

  // Reserve space for body
  if (this->rx_buf_.size() != this->rx_header_parsed_len_) {
    this->rx_buf_.resize(this->rx_header_parsed_len_);
  }

  if (rx_buf_len_ < rx_header_parsed_len_) {
    // more data to read
    uint16_t to_read = rx_header_parsed_len_ - rx_buf_len_;
    ssize_t received = this->socket_->read(&rx_buf_[rx_buf_len_], to_read);
    APIError err = handle_socket_read_result_(received);
    if (err != APIError::OK) {
      return err;
    }
    rx_buf_len_ += static_cast<uint16_t>(received);
    if (static_cast<uint16_t>(received) != to_read) {
      // not all read
      return APIError::WOULD_BLOCK;
    }
  }

  LOG_PACKET_RECEIVED(this->rx_buf_);

  // Clear state for next frame (rx_buf_ still contains data for caller)
  this->rx_buf_len_ = 0;
  this->rx_header_buf_pos_ = 0;
  this->rx_header_parsed_ = false;

  return APIError::OK;
}

APIError APIPlaintextFrameHelper::read_packet(ReadPacketBuffer *buffer) {
  if (this->state_ != State::DATA) {
    return APIError::WOULD_BLOCK;
  }

  APIError aerr = this->try_read_frame_();
  if (aerr != APIError::OK) {
    if (aerr == APIError::BAD_INDICATOR) {
      // Make sure to tell the remote that we don't
      // understand the indicator byte so it knows
      // we do not support it.
      struct iovec iov[1];
      // The \x00 first byte is the marker for plaintext.
      //
      // The remote will know how to handle the indicator byte,
      // but it likely won't understand the rest of the message.
      //
      // We must send at least 3 bytes to be read, so we add
      // a message after the indicator byte to ensures its long
      // enough and can aid in debugging.
      static constexpr uint8_t INDICATOR_MSG_SIZE = 19;
#ifdef USE_ESP8266
      static const char MSG_PROGMEM[] PROGMEM = "\x00"
                                                "Bad indicator byte";
      char msg[INDICATOR_MSG_SIZE];
      memcpy_P(msg, MSG_PROGMEM, INDICATOR_MSG_SIZE);
      iov[0].iov_base = (void *) msg;
#else
      static const char MSG[] = "\x00"
                                "Bad indicator byte";
      iov[0].iov_base = (void *) MSG;
#endif
      iov[0].iov_len = INDICATOR_MSG_SIZE;
      this->write_raw_(iov, 1, INDICATOR_MSG_SIZE);
    }
    return aerr;
  }

  buffer->data = this->rx_buf_.data();
  buffer->data_len = this->rx_header_parsed_len_;
  buffer->type = this->rx_header_parsed_type_;
  return APIError::OK;
}
APIError APIPlaintextFrameHelper::write_protobuf_packet(uint8_t type, ProtoWriteBuffer buffer) {
  PacketInfo packet{type, 0, static_cast<uint16_t>(buffer.get_buffer()->size() - frame_header_padding_)};
  return write_protobuf_packets(buffer, std::span<const PacketInfo>(&packet, 1));
}

APIError APIPlaintextFrameHelper::write_protobuf_packets(ProtoWriteBuffer buffer, std::span<const PacketInfo> packets) {
  if (state_ != State::DATA) {
    return APIError::BAD_STATE;
  }

  if (packets.empty()) {
    return APIError::OK;
  }

  uint8_t *buffer_data = buffer.get_buffer()->data();

  this->reusable_iovs_.clear();
  this->reusable_iovs_.reserve(packets.size());
  uint16_t total_write_len = 0;

  for (const auto &packet : packets) {
    // Calculate varint sizes for header layout
    uint8_t size_varint_len = api::ProtoSize::varint(static_cast<uint32_t>(packet.payload_size));
    uint8_t type_varint_len = api::ProtoSize::varint(static_cast<uint32_t>(packet.message_type));
    uint8_t total_header_len = 1 + size_varint_len + type_varint_len;

    // Calculate where to start writing the header
    // The header starts at the latest possible position to minimize unused padding
    //
    // Example 1 (small values): total_header_len = 3, header_offset = 6 - 3 = 3
    // [0-2]  - Unused padding
    // [3]    - 0x00 indicator byte
    // [4]    - Payload size varint (1 byte, for sizes 0-127)
    // [5]    - Message type varint (1 byte, for types 0-127)
    // [6...] - Actual payload data
    //
    // Example 2 (medium values): total_header_len = 4, header_offset = 6 - 4 = 2
    // [0-1]  - Unused padding
    // [2]    - 0x00 indicator byte
    // [3-4]  - Payload size varint (2 bytes, for sizes 128-16383)
    // [5]    - Message type varint (1 byte, for types 0-127)
    // [6...] - Actual payload data
    //
    // Example 3 (large values): total_header_len = 6, header_offset = 6 - 6 = 0
    // [0]    - 0x00 indicator byte
    // [1-3]  - Payload size varint (3 bytes, for sizes 16384-2097151)
    // [4-5]  - Message type varint (2 bytes, for types 128-32767)
    // [6...] - Actual payload data
    //
    // The message starts at offset + frame_header_padding_
    // So we write the header starting at offset + frame_header_padding_ - total_header_len
    uint8_t *buf_start = buffer_data + packet.offset;
    uint32_t header_offset = frame_header_padding_ - total_header_len;

    // Write the plaintext header
    buf_start[header_offset] = 0x00;  // indicator

    // Encode varints directly into buffer
    ProtoVarInt(packet.payload_size).encode_to_buffer_unchecked(buf_start + header_offset + 1, size_varint_len);
    ProtoVarInt(packet.message_type)
        .encode_to_buffer_unchecked(buf_start + header_offset + 1 + size_varint_len, type_varint_len);

    // Add iovec for this packet (header + payload)
    size_t packet_len = static_cast<size_t>(total_header_len + packet.payload_size);
    this->reusable_iovs_.push_back({buf_start + header_offset, packet_len});
    total_write_len += packet_len;
  }

  // Send all packets in one writev call
  return write_raw_(this->reusable_iovs_.data(), this->reusable_iovs_.size(), total_write_len);
}

}  // namespace esphome::api
#endif  // USE_API_PLAINTEXT
#endif  // USE_API
