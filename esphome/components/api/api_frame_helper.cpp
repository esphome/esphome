#include "api_frame_helper.h"
#ifdef USE_API
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "proto.h"
#include <cstring>
#include <cinttypes>

namespace esphome::api {

static const char *const TAG = "api.frame_helper";

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
#define LOG_PACKET_SENDING(data, len) \
  do { \
    char hex_buf_[format_hex_pretty_size(API_MAX_LOG_BYTES)]; \
    ESP_LOGVV(TAG, "Sending raw: %s", \
              format_hex_pretty_to(hex_buf_, data, (len) < API_MAX_LOG_BYTES ? (len) : API_MAX_LOG_BYTES)); \
  } while (0)
#else
#define LOG_PACKET_RECEIVED(buffer) ((void) 0)
#define LOG_PACKET_SENDING(data, len) ((void) 0)
#endif

const LogString *api_error_to_logstr(APIError err) {
  // not using switch to ensure compiler doesn't try to build a big table out of it
  if (err == APIError::OK) {
    return LOG_STR("OK");
  } else if (err == APIError::WOULD_BLOCK) {
    return LOG_STR("WOULD_BLOCK");
  } else if (err == APIError::BAD_INDICATOR) {
    return LOG_STR("BAD_INDICATOR");
  } else if (err == APIError::BAD_DATA_PACKET) {
    return LOG_STR("BAD_DATA_PACKET");
  } else if (err == APIError::TCP_NODELAY_FAILED) {
    return LOG_STR("TCP_NODELAY_FAILED");
  } else if (err == APIError::TCP_NONBLOCKING_FAILED) {
    return LOG_STR("TCP_NONBLOCKING_FAILED");
  } else if (err == APIError::CLOSE_FAILED) {
    return LOG_STR("CLOSE_FAILED");
  } else if (err == APIError::SHUTDOWN_FAILED) {
    return LOG_STR("SHUTDOWN_FAILED");
  } else if (err == APIError::BAD_STATE) {
    return LOG_STR("BAD_STATE");
  } else if (err == APIError::BAD_ARG) {
    return LOG_STR("BAD_ARG");
  } else if (err == APIError::SOCKET_READ_FAILED) {
    return LOG_STR("SOCKET_READ_FAILED");
  } else if (err == APIError::SOCKET_WRITE_FAILED) {
    return LOG_STR("SOCKET_WRITE_FAILED");
  } else if (err == APIError::OUT_OF_MEMORY) {
    return LOG_STR("OUT_OF_MEMORY");
  } else if (err == APIError::CONNECTION_CLOSED) {
    return LOG_STR("CONNECTION_CLOSED");
  }
#ifdef USE_API_NOISE
  else if (err == APIError::BAD_HANDSHAKE_PACKET_LEN) {
    return LOG_STR("BAD_HANDSHAKE_PACKET_LEN");
  } else if (err == APIError::HANDSHAKESTATE_READ_FAILED) {
    return LOG_STR("HANDSHAKESTATE_READ_FAILED");
  } else if (err == APIError::HANDSHAKESTATE_WRITE_FAILED) {
    return LOG_STR("HANDSHAKESTATE_WRITE_FAILED");
  } else if (err == APIError::HANDSHAKESTATE_BAD_STATE) {
    return LOG_STR("HANDSHAKESTATE_BAD_STATE");
  } else if (err == APIError::CIPHERSTATE_DECRYPT_FAILED) {
    return LOG_STR("CIPHERSTATE_DECRYPT_FAILED");
  } else if (err == APIError::CIPHERSTATE_ENCRYPT_FAILED) {
    return LOG_STR("CIPHERSTATE_ENCRYPT_FAILED");
  } else if (err == APIError::HANDSHAKESTATE_SETUP_FAILED) {
    return LOG_STR("HANDSHAKESTATE_SETUP_FAILED");
  } else if (err == APIError::HANDSHAKESTATE_SPLIT_FAILED) {
    return LOG_STR("HANDSHAKESTATE_SPLIT_FAILED");
  } else if (err == APIError::BAD_HANDSHAKE_ERROR_BYTE) {
    return LOG_STR("BAD_HANDSHAKE_ERROR_BYTE");
  }
#endif
  return LOG_STR("UNKNOWN");
}

#ifdef HELPER_LOG_PACKETS
void APIFrameHelper::log_packet_sending_(const void *data, uint16_t len) {
  LOG_PACKET_SENDING(reinterpret_cast<const uint8_t *>(data), len);
}
#endif

APIError APIFrameHelper::drain_overflow_and_handle_errors_() {
  if (this->overflow_buf_.try_drain(this->socket_.get()) == -1) {
    int err = errno;
    if (err != EWOULDBLOCK && err != EAGAIN) {
      this->state_ = State::FAILED;
      HELPER_LOG("Socket write failed with errno %d", err);
      return APIError::SOCKET_WRITE_FAILED;
    }
  }
  return APIError::OK;
}

// Single-buffer write path: wraps in iovec and delegates.
APIError APIFrameHelper::write_raw_buf_(const void *data, uint16_t len, ssize_t sent) {
  struct iovec iov = {const_cast<void *>(data), len};
  APIError err = this->write_raw_iov_(&iov, 1, len, sent);
#ifdef HELPER_LOG_PACKETS
  // Log after write/enqueue so re-entrant log sends can't corrupt data before it's sent
  if (err == APIError::OK)
    LOG_PACKET_SENDING(reinterpret_cast<const uint8_t *>(data), len);
#endif
  return err;
}

// Handles partial writes, errors, and overflow buffering.
// Called when the inline fast path couldn't complete the write,
// or directly from cold paths (handshake, error handling).
APIError APIFrameHelper::write_raw_iov_(const struct iovec *iov, int iovcnt, uint16_t total_write_len, ssize_t sent) {
  if (sent <= 0) {
    if (sent == WRITE_NOT_ATTEMPTED) {
      // Cold path: no write attempted yet, drain overflow and try
      if (!this->overflow_buf_.empty()) {
        APIError err = this->drain_overflow_and_handle_errors_();
        if (err != APIError::OK)
          return err;
      }
      if (this->overflow_buf_.empty()) {
        sent = this->write_iov_to_socket_(iov, iovcnt);
        if (sent == static_cast<ssize_t>(total_write_len))
          return APIError::OK;
        // Partial write or -1: fall through to error check / enqueue below
      } else {
        // Overflow backlog remains after drain; skip socket write, enqueue everything
        sent = 0;
      }
    }
    // WRITE_FAILED (-1): fast path or retry write returned -1, check errno
    if (sent == WRITE_FAILED) {
      int err = errno;
      if (err != EWOULDBLOCK && err != EAGAIN) {
        this->state_ = State::FAILED;
        HELPER_LOG("Socket write failed with errno %d", err);
        return APIError::SOCKET_WRITE_FAILED;
      }
      sent = 0;  // Treat WOULD_BLOCK as zero bytes sent
    }
  }

  // Full write completed (possible when called directly, not via write_raw_fast_buf_)
  if (sent == static_cast<ssize_t>(total_write_len))
    return APIError::OK;

  // Queue unsent data into overflow buffer
  if (!this->overflow_buf_.enqueue_iov(iov, iovcnt, total_write_len, static_cast<uint16_t>(sent))) {
    HELPER_LOG("Overflow buffer full, dropping connection");
    this->state_ = State::FAILED;
    return APIError::SOCKET_WRITE_FAILED;
  }
  return APIError::OK;
}

const char *APIFrameHelper::get_peername_to(std::span<char, socket::SOCKADDR_STR_LEN> buf) const {
  if (this->socket_) {
    this->socket_->getpeername_to(buf);
  } else {
    buf[0] = '\0';
  }
  return buf.data();
}

APIError APIFrameHelper::init_common_() {
  if (state_ != State::INITIALIZE || this->socket_ == nullptr) {
    HELPER_LOG("Bad state for init %d", (int) state_);
    return APIError::BAD_STATE;
  }
  int err = this->socket_->setblocking(false);
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("Setting nonblocking failed with errno %d", errno);
    return APIError::TCP_NONBLOCKING_FAILED;
  }

  int enable = 1;
  err = this->socket_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("Setting nodelay failed with errno %d", errno);
    return APIError::TCP_NODELAY_FAILED;
  }
  return APIError::OK;
}

APIError APIFrameHelper::handle_socket_read_result_(ssize_t received) {
  if (received == -1) {
    const int err = errno;
    if (err == EWOULDBLOCK || err == EAGAIN) {
      return APIError::WOULD_BLOCK;
    }
    state_ = State::FAILED;
    HELPER_LOG("Socket read failed with errno %d", err);
    return APIError::SOCKET_READ_FAILED;
  } else if (received == 0) {
    state_ = State::FAILED;
    HELPER_LOG("Connection closed");
    return APIError::CONNECTION_CLOSED;
  }
  return APIError::OK;
}

}  // namespace esphome::api
#endif
