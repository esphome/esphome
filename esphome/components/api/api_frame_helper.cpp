#include "api_frame_helper.h"
#ifdef USE_API
#include "api_connection.h"  // For ClientInfo struct
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "proto.h"
#include <cstring>
#include <cinttypes>

namespace esphome::api {

static const char *const TAG = "api.frame_helper";

#define HELPER_LOG(msg, ...) \
  ESP_LOGVV(TAG, "%s (%s): " msg, this->client_info_->name.c_str(), this->client_info_->peername.c_str(), ##__VA_ARGS__)

#ifdef HELPER_LOG_PACKETS
#define LOG_PACKET_RECEIVED(buffer) ESP_LOGVV(TAG, "Received frame: %s", format_hex_pretty(buffer).c_str())
#define LOG_PACKET_SENDING(data, len) ESP_LOGVV(TAG, "Sending raw: %s", format_hex_pretty(data, len).c_str())
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

// Default implementation for loop - handles sending buffered data
APIError APIFrameHelper::loop() {
  if (this->tx_buf_count_ > 0) {
    APIError err = try_send_tx_buf_();
    if (err != APIError::OK && err != APIError::WOULD_BLOCK) {
      return err;
    }
  }
  return APIError::OK;  // Convert WOULD_BLOCK to OK to avoid connection termination
}

// Common socket write error handling
APIError APIFrameHelper::handle_socket_write_error_() {
  if (errno == EWOULDBLOCK || errno == EAGAIN) {
    return APIError::WOULD_BLOCK;
  }
  HELPER_LOG("Socket write failed with errno %d", errno);
  this->state_ = State::FAILED;
  return APIError::SOCKET_WRITE_FAILED;
}

// Helper method to buffer data from IOVs
void APIFrameHelper::buffer_data_from_iov_(const struct iovec *iov, int iovcnt, uint16_t total_write_len,
                                           uint16_t offset) {
  // Check if queue is full
  if (this->tx_buf_count_ >= API_MAX_SEND_QUEUE) {
    HELPER_LOG("Send queue full (%u buffers), dropping connection", this->tx_buf_count_);
    this->state_ = State::FAILED;
    return;
  }

  uint16_t buffer_size = total_write_len - offset;
  auto &buffer = this->tx_buf_[this->tx_buf_tail_];
  buffer = std::make_unique<SendBuffer>(SendBuffer{
      .data = std::make_unique<uint8_t[]>(buffer_size),
      .size = buffer_size,
      .offset = 0,
  });

  uint16_t to_skip = offset;
  uint16_t write_pos = 0;

  for (int i = 0; i < iovcnt; i++) {
    if (to_skip >= iov[i].iov_len) {
      // Skip this entire segment
      to_skip -= static_cast<uint16_t>(iov[i].iov_len);
    } else {
      // Include this segment (partially or fully)
      const uint8_t *src = reinterpret_cast<uint8_t *>(iov[i].iov_base) + to_skip;
      uint16_t len = static_cast<uint16_t>(iov[i].iov_len) - to_skip;
      std::memcpy(buffer->data.get() + write_pos, src, len);
      write_pos += len;
      to_skip = 0;
    }
  }

  // Update circular buffer tracking
  this->tx_buf_tail_ = (this->tx_buf_tail_ + 1) % API_MAX_SEND_QUEUE;
  this->tx_buf_count_++;
}

// This method writes data to socket or buffers it
APIError APIFrameHelper::write_raw_(const struct iovec *iov, int iovcnt, uint16_t total_write_len) {
  // Returns APIError::OK if successful (or would block, but data has been buffered)
  // Returns APIError::SOCKET_WRITE_FAILED if socket write failed, and sets state to FAILED

  if (iovcnt == 0)
    return APIError::OK;  // Nothing to do, success

#ifdef HELPER_LOG_PACKETS
  for (int i = 0; i < iovcnt; i++) {
    LOG_PACKET_SENDING(reinterpret_cast<uint8_t *>(iov[i].iov_base), iov[i].iov_len);
  }
#endif

  // Try to send any existing buffered data first if there is any
  if (this->tx_buf_count_ > 0) {
    APIError send_result = try_send_tx_buf_();
    // If real error occurred (not just WOULD_BLOCK), return it
    if (send_result != APIError::OK && send_result != APIError::WOULD_BLOCK) {
      return send_result;
    }

    // If there is still data in the buffer, we can't send, buffer
    // the new data and return
    if (this->tx_buf_count_ > 0) {
      this->buffer_data_from_iov_(iov, iovcnt, total_write_len, 0);
      return APIError::OK;  // Success, data buffered
    }
  }

  // Try to send directly if no buffered data
  // Optimize for single iovec case (common for plaintext API)
  ssize_t sent =
      (iovcnt == 1) ? this->socket_->write(iov[0].iov_base, iov[0].iov_len) : this->socket_->writev(iov, iovcnt);

  if (sent == -1) {
    APIError err = this->handle_socket_write_error_();
    if (err == APIError::WOULD_BLOCK) {
      // Socket would block, buffer the data
      this->buffer_data_from_iov_(iov, iovcnt, total_write_len, 0);
      return APIError::OK;  // Success, data buffered
    }
    return err;  // Socket write failed
  } else if (static_cast<uint16_t>(sent) < total_write_len) {
    // Partially sent, buffer the remaining data
    this->buffer_data_from_iov_(iov, iovcnt, total_write_len, static_cast<uint16_t>(sent));
  }

  return APIError::OK;  // Success, all data sent or buffered
}

// Common implementation for trying to send buffered data
// IMPORTANT: Caller MUST ensure tx_buf_count_ > 0 before calling this method
APIError APIFrameHelper::try_send_tx_buf_() {
  // Try to send from tx_buf - we assume it's not empty as it's the caller's responsibility to check
  while (this->tx_buf_count_ > 0) {
    // Get the first buffer in the queue
    SendBuffer *front_buffer = this->tx_buf_[this->tx_buf_head_].get();

    // Try to send the remaining data in this buffer
    ssize_t sent = this->socket_->write(front_buffer->current_data(), front_buffer->remaining());

    if (sent == -1) {
      return this->handle_socket_write_error_();
    } else if (sent == 0) {
      // Nothing sent but not an error
      return APIError::WOULD_BLOCK;
    } else if (static_cast<uint16_t>(sent) < front_buffer->remaining()) {
      // Partially sent, update offset
      // Cast to ensure no overflow issues with uint16_t
      front_buffer->offset += static_cast<uint16_t>(sent);
      return APIError::WOULD_BLOCK;  // Stop processing more buffers if we couldn't send a complete buffer
    } else {
      // Buffer completely sent, remove it from the queue
      this->tx_buf_[this->tx_buf_head_].reset();
      this->tx_buf_head_ = (this->tx_buf_head_ + 1) % API_MAX_SEND_QUEUE;
      this->tx_buf_count_--;
      // Continue loop to try sending the next buffer
    }
  }

  return APIError::OK;  // All buffers sent successfully
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
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      return APIError::WOULD_BLOCK;
    }
    state_ = State::FAILED;
    HELPER_LOG("Socket read failed with errno %d", errno);
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
