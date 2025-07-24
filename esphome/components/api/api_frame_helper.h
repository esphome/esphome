#pragma once
#include <cstdint>
#include <deque>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "esphome/core/defines.h"
#ifdef USE_API
#include "esphome/components/socket/socket.h"
#include "esphome/core/application.h"
#include "esphome/core/log.h"

namespace esphome::api {

// uncomment to log raw packets
//#define HELPER_LOG_PACKETS

// Forward declaration
struct ClientInfo;

class ProtoWriteBuffer;

struct ReadPacketBuffer {
  std::vector<uint8_t> container;
  uint16_t type;
  uint16_t data_offset;
  uint16_t data_len;
};

// Packed packet info structure to minimize memory usage
struct PacketInfo {
  uint16_t offset;        // Offset in buffer where message starts
  uint16_t payload_size;  // Size of the message payload
  uint8_t message_type;   // Message type (0-255)

  PacketInfo(uint8_t type, uint16_t off, uint16_t size) : offset(off), payload_size(size), message_type(type) {}
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

const char *api_error_to_str(APIError err);

class APIFrameHelper {
 public:
  APIFrameHelper() = default;
  explicit APIFrameHelper(std::unique_ptr<socket::Socket> socket, const ClientInfo *client_info)
      : socket_owned_(std::move(socket)), client_info_(client_info) {
    socket_ = socket_owned_.get();
  }
  virtual ~APIFrameHelper() = default;
  virtual APIError init() = 0;
  virtual APIError loop();
  virtual APIError read_packet(ReadPacketBuffer *buffer) = 0;
  bool can_write_without_blocking() { return state_ == State::DATA && tx_buf_.empty(); }
  std::string getpeername() { return socket_->getpeername(); }
  int getpeername(struct sockaddr *addr, socklen_t *addrlen) { return socket_->getpeername(addr, addrlen); }
  APIError close() {
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
  virtual APIError write_protobuf_packet(uint8_t type, ProtoWriteBuffer buffer) = 0;
  // Write multiple protobuf packets in a single operation
  // packets contains (message_type, offset, length) for each message in the buffer
  // The buffer contains all messages with appropriate padding before each
  virtual APIError write_protobuf_packets(ProtoWriteBuffer buffer, std::span<const PacketInfo> packets) = 0;
  // Get the frame header padding required by this protocol
  virtual uint8_t frame_header_padding() = 0;
  // Get the frame footer size required by this protocol
  virtual uint8_t frame_footer_size() = 0;
  // Check if socket has data ready to read
  bool is_socket_ready() const { return socket_ != nullptr && socket_->ready(); }

 protected:
  // Buffer containing data to be sent
  struct SendBuffer {
    std::unique_ptr<uint8_t[]> data;
    uint16_t size{0};    // Total size of the buffer
    uint16_t offset{0};  // Current offset within the buffer

    // Using uint16_t reduces memory usage since ESPHome API messages are limited to UINT16_MAX (65535) bytes
    uint16_t remaining() const { return size - offset; }
    const uint8_t *current_data() const { return data.get() + offset; }
  };

  // Common implementation for writing raw data to socket
  APIError write_raw_(const struct iovec *iov, int iovcnt, uint16_t total_write_len);

  // Try to send data from the tx buffer
  APIError try_send_tx_buf_();

  // Helper method to buffer data from IOVs
  void buffer_data_from_iov_(const struct iovec *iov, int iovcnt, uint16_t total_write_len, uint16_t offset);

  // Common socket write error handling
  APIError handle_socket_write_error_();
  template<typename StateEnum>
  APIError write_raw_(const struct iovec *iov, int iovcnt, socket::Socket *socket, std::vector<uint8_t> &tx_buf,
                      const std::string &info, StateEnum &state, StateEnum failed_state);

  // Pointers first (4 bytes each)
  socket::Socket *socket_{nullptr};
  std::unique_ptr<socket::Socket> socket_owned_;

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

  // Containers (size varies, but typically 12+ bytes on 32-bit)
  std::deque<SendBuffer> tx_buf_;
  std::vector<struct iovec> reusable_iovs_;
  std::vector<uint8_t> rx_buf_;

  // Pointer to client info (4 bytes on 32-bit)
  // Note: The pointed-to ClientInfo object must outlive this APIFrameHelper instance.
  const ClientInfo *client_info_{nullptr};

  // Group smaller types together
  uint16_t rx_buf_len_ = 0;
  State state_{State::INITIALIZE};
  uint8_t frame_header_padding_{0};
  uint8_t frame_footer_size_{0};
  // 5 bytes total, 3 bytes padding

  // Common initialization for both plaintext and noise protocols
  APIError init_common_();

  // Helper method to handle socket read results
  APIError handle_socket_read_result_(ssize_t received);
};

}  // namespace esphome::api

#endif  // USE_API
