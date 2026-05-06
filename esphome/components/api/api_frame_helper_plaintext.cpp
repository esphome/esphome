#include "api_frame_helper_plaintext.h"
#ifdef USE_API
#ifdef USE_API_PLAINTEXT
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

// Maximum bytes to log in hex format (168 * 3 = 504, under TX buffer size of 512)
static constexpr size_t API_MAX_LOG_BYTES = 168;

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
#define HELPER_LOG(msg, ...) \
  do { \
    char peername_buf[socket::SOCKADDR_STR_LEN]; \
    this->get_peername_to(peername_buf); \
    ESP_LOGVV(TAG, "%s (%s): " msg, this->client_name_, peername_buf, ##__VA_ARGS__); \
  } while (0)
#else
#define HELPER_LOG(msg, ...) ((void) 0)
#endif

#ifdef HELPER_LOG_PACKETS
#define LOG_PACKET_RECEIVED(buffer) \
  do { \
    char hex_buf_[format_hex_pretty_size(API_MAX_LOG_BYTES)]; \
    ESP_LOGVV(TAG, "Received frame: %s", \
              format_hex_pretty_to(hex_buf_, (buffer).data(), \
                                   (buffer).size() < API_MAX_LOG_BYTES ? (buffer).size() : API_MAX_LOG_BYTES)); \
  } while (0)
#else
#define LOG_PACKET_RECEIVED(buffer) ((void) 0)
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
  if (!this->overflow_buf_.empty()) [[unlikely]] {
    return this->drain_overflow_and_handle_errors_();
  }
  return APIError::OK;
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

    // rx_header_buf_pos_ >= 3 and varint_pos == 1, so len >= 2
    auto msg_size_varint = ProtoVarInt::parse_non_empty(&rx_header_buf_[varint_pos], rx_header_buf_pos_ - varint_pos);
    if (!msg_size_varint.has_value()) {
      // not enough data there yet
      continue;
    }

    if (msg_size_varint.value > MAX_MESSAGE_SIZE) {
      state_ = State::FAILED;
      HELPER_LOG("Bad packet: message size %" PRIu32 " exceeds maximum %u",
                 static_cast<uint32_t>(msg_size_varint.value), MAX_MESSAGE_SIZE);
      return APIError::BAD_DATA_PACKET;
    }
    rx_header_parsed_len_ = static_cast<uint16_t>(msg_size_varint.value);

    // Move to next varint position
    varint_pos += msg_size_varint.consumed;

    auto msg_type_varint = ProtoVarInt::parse(&rx_header_buf_[varint_pos], rx_header_buf_pos_ - varint_pos);
    if (!msg_type_varint.has_value()) {
      // not enough data there yet
      continue;
    }
    if (msg_type_varint.value > std::numeric_limits<uint16_t>::max()) {
      state_ = State::FAILED;
      HELPER_LOG("Bad packet: message type %" PRIu32 " exceeds maximum %u",
                 static_cast<uint32_t>(msg_type_varint.value), std::numeric_limits<uint16_t>::max());
      return APIError::BAD_DATA_PACKET;
    }
    rx_header_parsed_type_ = static_cast<uint16_t>(msg_type_varint.value);
    rx_header_parsed_ = true;
  }
  // header reading done

  // Reserve space for body (+ null terminator so protobuf StringRef fields
  // can be safely null-terminated in-place after decode)
  this->rx_buf_.resize(this->rx_header_parsed_len_ + RX_BUF_NULL_TERMINATOR);

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
  APIError aerr = this->check_data_state_();
  if (aerr != APIError::OK)
    return aerr;

  aerr = this->try_read_frame_();
  if (aerr != APIError::OK) {
    if (aerr == APIError::BAD_INDICATOR) {
      // Make sure to tell the remote that we don't
      // understand the indicator byte so it knows
      // we do not support it.
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
      this->write_raw_buf_(msg, INDICATOR_MSG_SIZE);
#else
      static const char MSG[] = "\x00"
                                "Bad indicator byte";
      this->write_raw_buf_(MSG, INDICATOR_MSG_SIZE);
#endif
    }
    return aerr;
  }

  buffer->data = this->rx_buf_.data();
  buffer->data_len = this->rx_header_parsed_len_;
  buffer->type = this->rx_header_parsed_type_;
  return APIError::OK;
}

// Encode a 16-bit varint (1-3 bytes) using pre-computed length.
ESPHOME_ALWAYS_INLINE static inline void encode_varint_16(uint16_t value, uint8_t varint_len, uint8_t *p) {
  if (varint_len >= 2) {
    *p++ = static_cast<uint8_t>(value | 0x80);
    value >>= 7;
    if (varint_len == 3) {
      *p++ = static_cast<uint8_t>(value | 0x80);
      value >>= 7;
    }
  }
  *p = static_cast<uint8_t>(value);
}

// Encode an 8-bit varint (1-2 bytes) using pre-computed length.
ESPHOME_ALWAYS_INLINE static inline void encode_varint_8(uint8_t value, uint8_t varint_len, uint8_t *p) {
  if (varint_len == 2) {
    *p++ = static_cast<uint8_t>(value | 0x80);
    *p = static_cast<uint8_t>(value >> 7);
  } else {
    *p = value;
  }
}

// Write plaintext header into pre-allocated padding before payload.
// padding_size: bytes reserved before payload (HEADER_PADDING for first/single msg,
//               actual header size for contiguous batch messages).
// Returns the total header length (indicator + varints).
ESPHOME_ALWAYS_INLINE static inline uint8_t write_plaintext_header(uint8_t *buf_start, uint16_t payload_size,
                                                                   uint8_t message_type, uint8_t padding_size) {
  uint8_t size_varint_len = ProtoSize::varint16(payload_size);
  uint8_t type_varint_len = ProtoSize::varint8(message_type);
  uint8_t total_header_len = 1 + size_varint_len + type_varint_len;

  // The header is right-justified within the padding so it sits immediately before payload.
  //
  // Single/first message (padding_size = HEADER_PADDING = 6):
  //   Example (small, header=3): [0-2] unused | [3] 0x00 | [4] size | [5] type | [6...] payload
  //   Example (medium, header=4): [0-1] unused | [2] 0x00 | [3-4] size | [5] type | [6...] payload
  //   Example (large, header=6):  [0] 0x00 | [1-3] size | [4-5] type | [6...] payload
  //
  // Batch messages 2+ (padding_size = actual header size, no unused bytes):
  //   Example (small, header=3): [0] 0x00 | [1] size | [2] type | [3...] payload
  //   Example (medium, header=4): [0] 0x00 | [1-2] size | [3] type | [4...] payload
#ifdef ESPHOME_DEBUG_API
  assert(padding_size >= total_header_len);
#endif
  uint32_t header_offset = padding_size - total_header_len;

  // Write the plaintext header
  buf_start[header_offset] = 0x00;  // indicator

  // Encode varints directly into buffer using pre-computed lengths
  encode_varint_16(payload_size, size_varint_len, buf_start + header_offset + 1);
  encode_varint_8(message_type, type_varint_len, buf_start + header_offset + 1 + size_varint_len);

  return total_header_len;
}

APIError APIPlaintextFrameHelper::write_protobuf_packet(uint8_t type, ProtoWriteBuffer buffer) {
#ifdef ESPHOME_DEBUG_API
  assert(this->state_ == State::DATA);
#endif

  uint16_t payload_size = static_cast<uint16_t>(buffer.get_buffer()->size() - HEADER_PADDING);
  uint8_t *buffer_data = buffer.get_buffer()->data();
  uint8_t header_len = write_plaintext_header(buffer_data, payload_size, type, HEADER_PADDING);
  return this->write_raw_fast_buf_(buffer_data + HEADER_PADDING - header_len,
                                   static_cast<uint16_t>(header_len + payload_size));
}

APIError APIPlaintextFrameHelper::write_protobuf_messages(ProtoWriteBuffer buffer,
                                                          std::span<const MessageInfo> messages) {
#ifdef ESPHOME_DEBUG_API
  assert(this->state_ == State::DATA);
  assert(!messages.empty());
#endif
  uint8_t *buffer_data = buffer.get_buffer()->data();

  // First message has max padding (header_size = HEADER_PADDING), may have unused leading bytes.
  // Subsequent messages were encoded with exact header sizes (header_size = actual header len).
  // write_plaintext_header right-justifies the header within header_size bytes of padding.
  const auto &first = messages[0];
  uint8_t *first_start = buffer_data + first.offset;
  uint8_t header_len = write_plaintext_header(first_start, first.payload_size, first.message_type, HEADER_PADDING);
  uint8_t *write_start = first_start + HEADER_PADDING - header_len;
  uint16_t total_len = header_len + first.payload_size;

  for (size_t i = 1; i < messages.size(); i++) {
    const auto &msg = messages[i];
    header_len = write_plaintext_header(buffer_data + msg.offset, msg.payload_size, msg.message_type, msg.header_size);
    total_len += header_len + msg.payload_size;
  }

  return this->write_raw_fast_buf_(write_start, total_len);
}

}  // namespace esphome::api
#endif  // USE_API_PLAINTEXT
#endif  // USE_API
