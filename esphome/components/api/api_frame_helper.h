#pragma once
#include <array>
#include <cstdint>
#include <limits>
#include <memory>
#include <span>
#include <utility>

#include "esphome/core/defines.h"
#ifdef USE_API
#include "esphome/components/api/api_buffer.h"
#include "esphome/components/api/api_overflow_buffer.h"
#include "esphome/components/socket/socket.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"
#include "proto.h"

namespace esphome::api {

// uncomment to log raw packets
//#define HELPER_LOG_PACKETS

// Maximum message size limits to prevent OOM on constrained devices
// Handshake messages are limited to a small size for security
static constexpr uint16_t MAX_HANDSHAKE_SIZE = 128;

// Data message limits vary by platform based on available memory
#ifdef USE_ESP8266
static constexpr uint16_t MAX_MESSAGE_SIZE = 8192;  // 8 KiB for ESP8266
#else
static constexpr uint16_t MAX_MESSAGE_SIZE = 32768;  // 32 KiB for ESP32 and other platforms
#endif

// Extra byte reserved in rx_buf_ beyond the message size so protobuf
// StringRef fields can be null-terminated in-place after decode.
static constexpr uint16_t RX_BUF_NULL_TERMINATOR = 1;

// Maximum number of messages to batch in a single write operation
// Must be >= MAX_INITIAL_PER_BATCH in api_connection.h (enforced by static_assert there)
static constexpr size_t MAX_MESSAGES_PER_BATCH = 34;

// Max client name length (e.g., "Home Assistant 2026.1.0.dev0" = 28 chars)
static constexpr size_t CLIENT_INFO_NAME_MAX_LEN = 32;

struct ReadPacketBuffer {
  const uint8_t *data;  // Points directly into frame helper's rx_buf_ (valid until next read_packet call)
  uint16_t data_len;
  uint16_t type;
};

// Packed message info structure to minimize memory usage
// Note: message_type is uint8_t — all current protobuf message types fit in 8 bits.
// The noise wire format encodes types as 16-bit, but the high byte is always 0.
// If message types ever exceed 255, this and encrypt_noise_message_ must be updated.
struct MessageInfo {
  uint16_t offset;        // Offset in buffer where message starts
  uint16_t payload_size;  // Size of the message payload
  uint8_t message_type;   // Message type (0-255)
  uint8_t header_size;    // Actual header size used (avoids recomputation in write path)

  MessageInfo(uint8_t type, uint16_t off, uint16_t size, uint8_t hdr)
      : offset(off), payload_size(size), message_type(type), header_size(hdr) {}
};

enum class APIError : uint16_t {
  OK = 0,
  WOULD_BLOCK = 1001,
  BAD_INDICATOR = 1003,
  BAD_DATA_PACKET = 1004,
  TCP_NODELAY_FAILED = 1005,
  TCP_NONBLOCKING_FAILED = 1006,
  CLOSE_FAILED = 1007,
  SHUTDOWN_FAILED = 1008,
  BAD_STATE = 1009,
  BAD_ARG = 1010,
  SOCKET_READ_FAILED = 1011,
  SOCKET_WRITE_FAILED = 1012,
  OUT_OF_MEMORY = 1018,
  CONNECTION_CLOSED = 1022,
#ifdef USE_API_NOISE
  BAD_HANDSHAKE_PACKET_LEN = 1002,
  HANDSHAKESTATE_READ_FAILED = 1013,
  HANDSHAKESTATE_WRITE_FAILED = 1014,
  HANDSHAKESTATE_BAD_STATE = 1015,
  CIPHERSTATE_DECRYPT_FAILED = 1016,
  CIPHERSTATE_ENCRYPT_FAILED = 1017,
  HANDSHAKESTATE_SETUP_FAILED = 1019,
  HANDSHAKESTATE_SPLIT_FAILED = 1020,
  BAD_HANDSHAKE_ERROR_BYTE = 1021,
#endif
};

const LogString *api_error_to_logstr(APIError err);

class APIFrameHelper {
 public:
  APIFrameHelper() = default;
  explicit APIFrameHelper(std::unique_ptr<socket::Socket> socket) : socket_(std::move(socket)) {}

  // Get client name (null-terminated)
  const char *get_client_name() const { return this->client_name_; }
  // Get client peername/IP into caller-provided buffer (fetches on-demand from socket)
  // Returns pointer to buf for convenience in printf-style calls
  const char *get_peername_to(std::span<char, socket::SOCKADDR_STR_LEN> buf) const;
  // Set client name from buffer with length (truncates if needed)
  void set_client_name(const char *name, size_t len) {
    size_t copy_len = std::min(len, sizeof(this->client_name_) - 1);
    memcpy(this->client_name_, name, copy_len);
    this->client_name_[copy_len] = '\0';
  }
  virtual ~APIFrameHelper() = default;
  virtual APIError init() = 0;
  virtual APIError loop() = 0;
  virtual APIError read_packet(ReadPacketBuffer *buffer) = 0;
  bool can_write_without_blocking() { return this->state_ == State::DATA && this->overflow_buf_.empty(); }
  int getpeername(struct sockaddr *addr, socklen_t *addrlen) { return socket_->getpeername(addr, addrlen); }
  APIError close() {
    if (state_ == State::CLOSED)
      return APIError::OK;  // Already closed
    state_ = State::CLOSED;
    int err = this->socket_->close();
    if (err == -1)
      return APIError::CLOSE_FAILED;
    return APIError::OK;
  }
  APIError shutdown(int how) {
    int err = this->socket_->shutdown(how);
    if (err == -1)
      return APIError::SHUTDOWN_FAILED;
    if (how == SHUT_RDWR) {
      state_ = State::CLOSED;
    }
    return APIError::OK;
  }
  // Manage TCP_NODELAY (Nagle's algorithm) based on message type.
  //
  // For non-log messages (sensor data, state updates): Always disable Nagle
  // (NODELAY on) for immediate delivery - these are time-sensitive.
  //
  // For log messages: Use Nagle to coalesce multiple small log packets into
  // fewer larger packets, reducing WiFi overhead. However, we limit batching
  // to avoid excessive LWIP buffer pressure on memory-constrained devices.
  // LWIP's TCP_OVERSIZE option coalesces the data into shared pbufs, but
  // holding data too long waiting for Nagle's timer causes buffer exhaustion
  // and dropped messages.
  //
  // ESP32 (TCP_SND_BUF=4×MSS+) / RP2040 (8×MSS) / LibreTiny (4×MSS): 4 logs per cycle
  // ESP8266 (2×MSS): 3 logs per cycle (tightest buffers)
  //
  // Flow (ESP32/RP2040/LT): Log 1 (Nagle on) -> Log 2 -> Log 3 -> Log 4 (NODELAY, flush)
  // Flow (ESP8266):         Log 1 (Nagle on) -> Log 2 -> Log 3 (NODELAY, flush all)
  //
  void set_nodelay_for_message(bool is_log_message) {
    if (!is_log_message) {
      if (this->nodelay_counter_) {
        this->set_nodelay_raw_(true);
        this->nodelay_counter_ = 0;
      }
      return;
    }
    // Log message: enable Nagle on first, flush after LOG_NAGLE_COUNT
    if (!this->nodelay_counter_)
      this->set_nodelay_raw_(false);
    if (++this->nodelay_counter_ > LOG_NAGLE_COUNT) {
      this->set_nodelay_raw_(true);
      this->nodelay_counter_ = 0;
    }
  }
  // Write a single protobuf message - the hot path (87-100% of all writes).
  // Caller must ensure state is DATA before calling.
  virtual APIError write_protobuf_packet(uint8_t type, ProtoWriteBuffer buffer) = 0;
  // Write multiple protobuf messages in a single batched operation.
  // Caller must ensure state is DATA and messages is not empty.
  // messages contains (message_type, offset, length) for each message in the buffer.
  // The buffer contains all messages with appropriate padding before each.
  virtual APIError write_protobuf_messages(ProtoWriteBuffer buffer, std::span<const MessageInfo> messages) = 0;
  // Get the maximum frame header padding required by this protocol (worst case)
  uint8_t frame_header_padding() const { return frame_header_padding_; }
  // Get the actual frame header size for a specific message.
  // For noise: always returns frame_header_padding_ (fixed 7-byte header).
  // For plaintext: computes actual size from varint lengths (3-6 bytes).
  // Distinguishes protocols via frame_footer_size_ (noise always has a non-zero MAC
  // footer, plaintext has footer=0). If a protocol with a plaintext footer is ever
  // added, this should become a virtual method.
  uint8_t frame_header_size(uint16_t payload_size, uint8_t message_type) const {
#if defined(USE_API_NOISE) && defined(USE_API_PLAINTEXT)
    return this->frame_footer_size_
               ? this->frame_header_padding_
               : static_cast<uint8_t>(1 + ProtoSize::varint16(payload_size) + ProtoSize::varint8(message_type));
#elif defined(USE_API_NOISE)
    return this->frame_header_padding_;
#else  // USE_API_PLAINTEXT only
    return static_cast<uint8_t>(1 + ProtoSize::varint16(payload_size) + ProtoSize::varint8(message_type));
#endif
  }
  // Get the frame footer size required by this protocol
  uint8_t frame_footer_size() const { return frame_footer_size_; }
  // Check if socket has buffered data ready to read.
  // Contract: callers must read until it would block (EAGAIN/EWOULDBLOCK)
  // or track that they stopped early and retry without this check.
  // See Socket::ready() for details.
  bool is_socket_ready() const { return socket_ != nullptr && socket_->ready(); }
  // Release excess memory from internal buffers after initial sync
  void release_buffers() {
    // rx_buf_: Safe to clear only if no partial read in progress.
    // rx_buf_len_ tracks bytes read so far; if non-zero, we're mid-frame
    // and clearing would lose partially received data.
    if (this->rx_buf_len_ == 0) {
      this->rx_buf_.release();
    }
  }

 protected:
  // Drain backlogged overflow data to the socket and handle errors.
  // Called when overflow_buf_.empty() is false. Out-of-line to keep the
  // fast path (empty check) inline at call sites.
  // Returns OK for transient errors (WOULD_BLOCK), SOCKET_WRITE_FAILED for hard errors.
  APIError drain_overflow_and_handle_errors_();

  // Sentinel values for the sent parameter in write_raw_ methods
  static constexpr ssize_t WRITE_FAILED = -1;         // Fast path: write()/writev() returned -1
  static constexpr ssize_t WRITE_NOT_ATTEMPTED = -2;  // Cold path: no write attempted yet

  // Dispatch to write() or writev() based on iovec count
  inline ssize_t ESPHOME_ALWAYS_INLINE write_iov_to_socket_(const struct iovec *iov, int iovcnt) {
    return (iovcnt == 1) ? this->socket_->write(iov[0].iov_base, iov[0].iov_len) : this->socket_->writev(iov, iovcnt);
  }

  // Inlined write methods — used by hot paths (write_protobuf_packet, write_protobuf_messages)
  // These inline the fast path (overflow empty + full write) and tail-call the out-of-line
  // slow path only on failure/partial write.
  inline APIError ESPHOME_ALWAYS_INLINE write_raw_fast_buf_(const void *data, uint16_t len) {
    if (this->overflow_buf_.empty()) [[likely]] {
      ssize_t sent = this->socket_->write(data, len);
      if (sent == static_cast<ssize_t>(len)) [[likely]] {
#ifdef HELPER_LOG_PACKETS
        this->log_packet_sending_(data, len);
#endif
        return APIError::OK;
      }
      // sent is -1 (WRITE_FAILED) or partial write count
      return this->write_raw_buf_(data, len, sent);
    }
    return this->write_raw_buf_(data, len, WRITE_NOT_ATTEMPTED);
  }
  // Out-of-line write paths: handle partial writes, errors, overflow buffering
  // sent: WRITE_NOT_ATTEMPTED (cold path), WRITE_FAILED (fast path write returned -1), or bytes sent (partial write)
  APIError write_raw_buf_(const void *data, uint16_t len, ssize_t sent = WRITE_NOT_ATTEMPTED);
  APIError write_raw_iov_(const struct iovec *iov, int iovcnt, uint16_t total_write_len,
                          ssize_t sent = WRITE_NOT_ATTEMPTED);
#ifdef HELPER_LOG_PACKETS
  void log_packet_sending_(const void *data, uint16_t len);
#endif

  // Socket ownership (4 bytes on 32-bit, 8 bytes on 64-bit)
  std::unique_ptr<socket::Socket> socket_;

  // Common state enum for all frame helpers
  // Note: Not all states are used by all implementations
  // - INITIALIZE: Used by both Noise and Plaintext
  // - CLIENT_HELLO, SERVER_HELLO, HANDSHAKE: Only used by Noise protocol
  // - DATA: Used by both Noise and Plaintext
  // - CLOSED: Used by both Noise and Plaintext
  // - FAILED: Used by both Noise and Plaintext
  // - EXPLICIT_REJECT: Only used by Noise protocol
  enum class State : uint8_t {
    INITIALIZE = 1,
    CLIENT_HELLO = 2,  // Noise only
    SERVER_HELLO = 3,  // Noise only
    HANDSHAKE = 4,     // Noise only
    DATA = 5,
    CLOSED = 6,
    FAILED = 7,
    EXPLICIT_REJECT = 8,  // Noise only
  };

  // Fast inline state check for read_packet/write_protobuf_messages hot path.
  // Returns OK only in DATA state; maps CLOSED/FAILED to BAD_STATE and any
  // other intermediate state to WOULD_BLOCK.
  inline APIError ESPHOME_ALWAYS_INLINE check_data_state_() const {
    if (this->state_ == State::DATA)
      return APIError::OK;
    if (this->state_ == State::CLOSED || this->state_ == State::FAILED)
      return APIError::BAD_STATE;
    return APIError::WOULD_BLOCK;
  }

  // Backlog for unsent data when TCP send buffer is full (rarely used in production)
  APIOverflowBuffer overflow_buf_;
  APIBuffer rx_buf_;

  // Client name buffer - stores name from Hello message or initial peername
  char client_name_[CLIENT_INFO_NAME_MAX_LEN]{};

  // Group smaller types together
  uint16_t rx_buf_len_ = 0;
  State state_{State::INITIALIZE};
  uint8_t frame_header_padding_{0};
  uint8_t frame_footer_size_{0};
  // Nagle batching counter for log messages. 0 means NODELAY is enabled (immediate send).
  // Values 1..LOG_NAGLE_COUNT count log messages in the current Nagle batch.
  // After LOG_NAGLE_COUNT logs, we flush by re-enabling NODELAY and resetting to 0.
  // ESP8266 has the tightest TCP send buffer (2×MSS) and needs conservative batching.
  // ESP32 (4×MSS+), RP2040 (8×MSS), and LibreTiny (4×MSS) can coalesce more.
#ifdef USE_ESP8266
  static constexpr uint8_t LOG_NAGLE_COUNT = 2;
#else
  static constexpr uint8_t LOG_NAGLE_COUNT = 3;
#endif
  uint8_t nodelay_counter_{0};

  // Internal helper to set TCP_NODELAY socket option
  void set_nodelay_raw_(bool enable) {
    int val = enable ? 1 : 0;
    this->socket_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &val, sizeof(int));
  }

  // Common initialization for both plaintext and noise protocols
  APIError init_common_();

  // Helper method to handle socket read results
  APIError handle_socket_read_result_(ssize_t received);
};

}  // namespace esphome::api

#endif  // USE_API
