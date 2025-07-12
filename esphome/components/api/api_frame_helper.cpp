#include "api_frame_helper.h"
#ifdef USE_API
#include "esphome/core/application.h"
#include "esphome/core/hal.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "proto.h"
#include <cstring>
#include <cinttypes>

namespace esphome {
namespace api {

static const char *const TAG = "api.socket";

const char *api_error_to_str(APIError err) {
  // not using switch to ensure compiler doesn't try to build a big table out of it
  if (err == APIError::OK) {
    return "OK";
  } else if (err == APIError::WOULD_BLOCK) {
    return "WOULD_BLOCK";
  } else if (err == APIError::BAD_HANDSHAKE_PACKET_LEN) {
    return "BAD_HANDSHAKE_PACKET_LEN";
  } else if (err == APIError::BAD_INDICATOR) {
    return "BAD_INDICATOR";
  } else if (err == APIError::BAD_DATA_PACKET) {
    return "BAD_DATA_PACKET";
  } else if (err == APIError::TCP_NODELAY_FAILED) {
    return "TCP_NODELAY_FAILED";
  } else if (err == APIError::TCP_NONBLOCKING_FAILED) {
    return "TCP_NONBLOCKING_FAILED";
  } else if (err == APIError::CLOSE_FAILED) {
    return "CLOSE_FAILED";
  } else if (err == APIError::SHUTDOWN_FAILED) {
    return "SHUTDOWN_FAILED";
  } else if (err == APIError::BAD_STATE) {
    return "BAD_STATE";
  } else if (err == APIError::BAD_ARG) {
    return "BAD_ARG";
  } else if (err == APIError::SOCKET_READ_FAILED) {
    return "SOCKET_READ_FAILED";
  } else if (err == APIError::SOCKET_WRITE_FAILED) {
    return "SOCKET_WRITE_FAILED";
  } else if (err == APIError::HANDSHAKESTATE_READ_FAILED) {
    return "HANDSHAKESTATE_READ_FAILED";
  } else if (err == APIError::HANDSHAKESTATE_WRITE_FAILED) {
    return "HANDSHAKESTATE_WRITE_FAILED";
  } else if (err == APIError::HANDSHAKESTATE_BAD_STATE) {
    return "HANDSHAKESTATE_BAD_STATE";
  } else if (err == APIError::CIPHERSTATE_DECRYPT_FAILED) {
    return "CIPHERSTATE_DECRYPT_FAILED";
  } else if (err == APIError::CIPHERSTATE_ENCRYPT_FAILED) {
    return "CIPHERSTATE_ENCRYPT_FAILED";
  } else if (err == APIError::OUT_OF_MEMORY) {
    return "OUT_OF_MEMORY";
  } else if (err == APIError::HANDSHAKESTATE_SETUP_FAILED) {
    return "HANDSHAKESTATE_SETUP_FAILED";
  } else if (err == APIError::HANDSHAKESTATE_SPLIT_FAILED) {
    return "HANDSHAKESTATE_SPLIT_FAILED";
  } else if (err == APIError::BAD_HANDSHAKE_ERROR_BYTE) {
    return "BAD_HANDSHAKE_ERROR_BYTE";
  } else if (err == APIError::CONNECTION_CLOSED) {
    return "CONNECTION_CLOSED";
  }
  return "UNKNOWN";
}

// Default implementation for loop - handles sending buffered data
APIError APIFrameHelper::loop() {
  if (!this->tx_buf_.empty()) {
    APIError err = try_send_tx_buf_();
    if (err != APIError::OK && err != APIError::WOULD_BLOCK) {
      return err;
    }
  }
  return APIError::OK;  // Convert WOULD_BLOCK to OK to avoid connection termination
}

// Helper method to buffer data from IOVs
void APIFrameHelper::buffer_data_from_iov_(const struct iovec *iov, int iovcnt, uint16_t total_write_len) {
  SendBuffer buffer;
  buffer.data.reserve(total_write_len);
  for (int i = 0; i < iovcnt; i++) {
    const uint8_t *data = reinterpret_cast<uint8_t *>(iov[i].iov_base);
    buffer.data.insert(buffer.data.end(), data, data + iov[i].iov_len);
  }
  this->tx_buf_.push_back(std::move(buffer));
}

// This method writes data to socket or buffers it
APIError APIFrameHelper::write_raw_(const struct iovec *iov, int iovcnt) {
  // Returns APIError::OK if successful (or would block, but data has been buffered)
  // Returns APIError::SOCKET_WRITE_FAILED if socket write failed, and sets state to FAILED

  if (iovcnt == 0)
    return APIError::OK;  // Nothing to do, success

  uint16_t total_write_len = 0;
  for (int i = 0; i < iovcnt; i++) {
#ifdef HELPER_LOG_PACKETS
    ESP_LOGVV(TAG, "Sending raw: %s",
              format_hex_pretty(reinterpret_cast<uint8_t *>(iov[i].iov_base), iov[i].iov_len).c_str());
#endif
    total_write_len += static_cast<uint16_t>(iov[i].iov_len);
  }

  // Try to send any existing buffered data first if there is any
  if (!this->tx_buf_.empty()) {
    APIError send_result = try_send_tx_buf_();
    // If real error occurred (not just WOULD_BLOCK), return it
    if (send_result != APIError::OK && send_result != APIError::WOULD_BLOCK) {
      return send_result;
    }

    // If there is still data in the buffer, we can't send, buffer
    // the new data and return
    if (!this->tx_buf_.empty()) {
      this->buffer_data_from_iov_(iov, iovcnt, total_write_len);
      return APIError::OK;  // Success, data buffered
    }
  }

  // Try to send directly if no buffered data
  ssize_t sent = this->socket_->writev(iov, iovcnt);

  if (sent == -1) {
    if (errno == EWOULDBLOCK || errno == EAGAIN) {
      // Socket would block, buffer the data
      this->buffer_data_from_iov_(iov, iovcnt, total_write_len);
      return APIError::OK;  // Success, data buffered
    }
    // Socket error
    ESP_LOGVV(TAG, "%s: Socket write failed with errno %d", this->info_.c_str(), errno);
    this->state_ = State::FAILED;
    return APIError::SOCKET_WRITE_FAILED;  // Socket write failed
  } else if (static_cast<uint16_t>(sent) < total_write_len) {
    // Partially sent, buffer the remaining data
    SendBuffer buffer;
    uint16_t to_consume = static_cast<uint16_t>(sent);
    uint16_t remaining = total_write_len - static_cast<uint16_t>(sent);

    buffer.data.reserve(remaining);

    for (int i = 0; i < iovcnt; i++) {
      if (to_consume >= iov[i].iov_len) {
        // This segment was fully sent
        to_consume -= static_cast<uint16_t>(iov[i].iov_len);
      } else {
        // This segment was partially sent or not sent at all
        const uint8_t *data = reinterpret_cast<uint8_t *>(iov[i].iov_base) + to_consume;
        uint16_t len = static_cast<uint16_t>(iov[i].iov_len) - to_consume;
        buffer.data.insert(buffer.data.end(), data, data + len);
        to_consume = 0;
      }
    }

    this->tx_buf_.push_back(std::move(buffer));
  }

  return APIError::OK;  // Success, all data sent or buffered
}

// Common implementation for trying to send buffered data
// IMPORTANT: Caller MUST ensure tx_buf_ is not empty before calling this method
APIError APIFrameHelper::try_send_tx_buf_() {
  // Try to send from tx_buf - we assume it's not empty as it's the caller's responsibility to check
  bool tx_buf_empty = false;
  while (!tx_buf_empty) {
    // Get the first buffer in the queue
    SendBuffer &front_buffer = this->tx_buf_.front();

    // Try to send the remaining data in this buffer
    ssize_t sent = this->socket_->write(front_buffer.current_data(), front_buffer.remaining());

    if (sent == -1) {
      if (errno != EWOULDBLOCK && errno != EAGAIN) {
        // Real socket error (not just would block)
        ESP_LOGVV(TAG, "%s: Socket write failed with errno %d", this->info_.c_str(), errno);
        this->state_ = State::FAILED;
        return APIError::SOCKET_WRITE_FAILED;  // Socket write failed
      }
      // Socket would block, we'll try again later
      return APIError::WOULD_BLOCK;
    } else if (sent == 0) {
      // Nothing sent but not an error
      return APIError::WOULD_BLOCK;
    } else if (static_cast<uint16_t>(sent) < front_buffer.remaining()) {
      // Partially sent, update offset
      // Cast to ensure no overflow issues with uint16_t
      front_buffer.offset += static_cast<uint16_t>(sent);
      return APIError::WOULD_BLOCK;  // Stop processing more buffers if we couldn't send a complete buffer
    } else {
      // Buffer completely sent, remove it from the queue
      this->tx_buf_.pop_front();
      // Update empty status for the loop condition
      tx_buf_empty = this->tx_buf_.empty();
      // Continue loop to try sending the next buffer
    }
  }

  return APIError::OK;  // All buffers sent successfully
}

APIError APIFrameHelper::init_common_() {
  if (state_ != State::INITIALIZE || this->socket_ == nullptr) {
    ESP_LOGVV(TAG, "%s: Bad state for init %d", this->info_.c_str(), (int) state_);
    return APIError::BAD_STATE;
  }
  int err = this->socket_->setblocking(false);
  if (err != 0) {
    state_ = State::FAILED;
    ESP_LOGVV(TAG, "%s: Setting nonblocking failed with errno %d", this->info_.c_str(), errno);
    return APIError::TCP_NONBLOCKING_FAILED;
  }

  int enable = 1;
  err = this->socket_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
  if (err != 0) {
    state_ = State::FAILED;
    ESP_LOGVV(TAG, "%s: Setting nodelay failed with errno %d", this->info_.c_str(), errno);
    return APIError::TCP_NODELAY_FAILED;
  }
  return APIError::OK;
}

#define HELPER_LOG(msg, ...) ESP_LOGVV(TAG, "%s: " msg, this->info_.c_str(), ##__VA_ARGS__)

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
// uncomment to log raw packets
//#define HELPER_LOG_PACKETS

#ifdef USE_API_NOISE
static const char *const PROLOGUE_INIT = "NoiseAPIInit";

/// Convert a noise error code to a readable error
std::string noise_err_to_str(int err) {
  if (err == NOISE_ERROR_NO_MEMORY)
    return "NO_MEMORY";
  if (err == NOISE_ERROR_UNKNOWN_ID)
    return "UNKNOWN_ID";
  if (err == NOISE_ERROR_UNKNOWN_NAME)
    return "UNKNOWN_NAME";
  if (err == NOISE_ERROR_MAC_FAILURE)
    return "MAC_FAILURE";
  if (err == NOISE_ERROR_NOT_APPLICABLE)
    return "NOT_APPLICABLE";
  if (err == NOISE_ERROR_SYSTEM)
    return "SYSTEM";
  if (err == NOISE_ERROR_REMOTE_KEY_REQUIRED)
    return "REMOTE_KEY_REQUIRED";
  if (err == NOISE_ERROR_LOCAL_KEY_REQUIRED)
    return "LOCAL_KEY_REQUIRED";
  if (err == NOISE_ERROR_PSK_REQUIRED)
    return "PSK_REQUIRED";
  if (err == NOISE_ERROR_INVALID_LENGTH)
    return "INVALID_LENGTH";
  if (err == NOISE_ERROR_INVALID_PARAM)
    return "INVALID_PARAM";
  if (err == NOISE_ERROR_INVALID_STATE)
    return "INVALID_STATE";
  if (err == NOISE_ERROR_INVALID_NONCE)
    return "INVALID_NONCE";
  if (err == NOISE_ERROR_INVALID_PRIVATE_KEY)
    return "INVALID_PRIVATE_KEY";
  if (err == NOISE_ERROR_INVALID_PUBLIC_KEY)
    return "INVALID_PUBLIC_KEY";
  if (err == NOISE_ERROR_INVALID_FORMAT)
    return "INVALID_FORMAT";
  if (err == NOISE_ERROR_INVALID_SIGNATURE)
    return "INVALID_SIGNATURE";
  return to_string(err);
}

/// Initialize the frame helper, returns OK if successful.
APIError APINoiseFrameHelper::init() {
  APIError err = init_common_();
  if (err != APIError::OK) {
    return err;
  }

  // init prologue
  prologue_.insert(prologue_.end(), PROLOGUE_INIT, PROLOGUE_INIT + strlen(PROLOGUE_INIT));

  state_ = State::CLIENT_HELLO;
  return APIError::OK;
}
/// Run through handshake messages (if in that phase)
APIError APINoiseFrameHelper::loop() {
  // During handshake phase, process as many actions as possible until we can't progress
  // socket_->ready() stays true until next main loop, but state_action() will return
  // WOULD_BLOCK when no more data is available to read
  while (state_ != State::DATA && this->socket_->ready()) {
    APIError err = state_action_();
    if (err != APIError::OK && err != APIError::WOULD_BLOCK) {
      return err;
    }
    if (err == APIError::WOULD_BLOCK) {
      break;
    }
  }

  // Use base class implementation for buffer sending
  return APIFrameHelper::loop();
}

/** Read a packet into the rx_buf_. If successful, stores frame data in the frame parameter
 *
 * @param frame: The struct to hold the frame information in.
 *   msg_start: points to the start of the payload - this pointer is only valid until the next
 *     try_receive_raw_ call
 *
 * @return 0 if a full packet is in rx_buf_
 * @return -1 if error, check errno.
 *
 * errno EWOULDBLOCK: Packet could not be read without blocking. Try again later.
 * errno ENOMEM: Not enough memory for reading packet.
 * errno API_ERROR_BAD_INDICATOR: Bad indicator byte at start of frame.
 * errno API_ERROR_HANDSHAKE_PACKET_LEN: Packet too big for this phase.
 */
APIError APINoiseFrameHelper::try_read_frame_(ParsedFrame *frame) {
  if (frame == nullptr) {
    HELPER_LOG("Bad argument for try_read_frame_");
    return APIError::BAD_ARG;
  }

  // read header
  if (rx_header_buf_len_ < 3) {
    // no header information yet
    uint8_t to_read = 3 - rx_header_buf_len_;
    ssize_t received = this->socket_->read(&rx_header_buf_[rx_header_buf_len_], to_read);
    APIError err = handle_socket_read_result_(received);
    if (err != APIError::OK) {
      return err;
    }
    rx_header_buf_len_ += static_cast<uint8_t>(received);
    if (static_cast<uint8_t>(received) != to_read) {
      // not a full read
      return APIError::WOULD_BLOCK;
    }

    if (rx_header_buf_[0] != 0x01) {
      state_ = State::FAILED;
      HELPER_LOG("Bad indicator byte %u", rx_header_buf_[0]);
      return APIError::BAD_INDICATOR;
    }
    // header reading done
  }

  // read body
  uint16_t msg_size = (((uint16_t) rx_header_buf_[1]) << 8) | rx_header_buf_[2];

  if (state_ != State::DATA && msg_size > 128) {
    // for handshake message only permit up to 128 bytes
    state_ = State::FAILED;
    HELPER_LOG("Bad packet len for handshake: %d", msg_size);
    return APIError::BAD_HANDSHAKE_PACKET_LEN;
  }

  // reserve space for body
  if (rx_buf_.size() != msg_size) {
    rx_buf_.resize(msg_size);
  }

  if (rx_buf_len_ < msg_size) {
    // more data to read
    uint16_t to_read = msg_size - rx_buf_len_;
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

  // uncomment for even more debugging
#ifdef HELPER_LOG_PACKETS
  ESP_LOGVV(TAG, "Received frame: %s", format_hex_pretty(rx_buf_).c_str());
#endif
  frame->msg = std::move(rx_buf_);
  // consume msg
  rx_buf_ = {};
  rx_buf_len_ = 0;
  rx_header_buf_len_ = 0;
  return APIError::OK;
}

/** To be called from read/write methods.
 *
 * This method runs through the internal handshake methods, if in that state.
 *
 * If the handshake is still active when this method returns and a read/write can't take place at
 * the moment, returns WOULD_BLOCK.
 * If an error occurred, returns that error. Only returns OK if the transport is ready for data
 * traffic.
 */
APIError APINoiseFrameHelper::state_action_() {
  int err;
  APIError aerr;
  if (state_ == State::INITIALIZE) {
    HELPER_LOG("Bad state for method: %d", (int) state_);
    return APIError::BAD_STATE;
  }
  if (state_ == State::CLIENT_HELLO) {
    // waiting for client hello
    ParsedFrame frame;
    aerr = try_read_frame_(&frame);
    if (aerr == APIError::BAD_INDICATOR) {
      send_explicit_handshake_reject_("Bad indicator byte");
      return aerr;
    }
    if (aerr == APIError::BAD_HANDSHAKE_PACKET_LEN) {
      send_explicit_handshake_reject_("Bad handshake packet len");
      return aerr;
    }
    if (aerr != APIError::OK)
      return aerr;
    // ignore contents, may be used in future for flags
    // Reserve space for: existing prologue + 2 size bytes + frame data
    prologue_.reserve(prologue_.size() + 2 + frame.msg.size());
    prologue_.push_back((uint8_t) (frame.msg.size() >> 8));
    prologue_.push_back((uint8_t) frame.msg.size());
    prologue_.insert(prologue_.end(), frame.msg.begin(), frame.msg.end());

    state_ = State::SERVER_HELLO;
  }
  if (state_ == State::SERVER_HELLO) {
    // send server hello
    const std::string &name = App.get_name();
    const std::string &mac = get_mac_address();

    std::vector<uint8_t> msg;
    // Reserve space for: 1 byte proto + name + null + mac + null
    msg.reserve(1 + name.size() + 1 + mac.size() + 1);

    // chosen proto
    msg.push_back(0x01);

    // node name, terminated by null byte
    const uint8_t *name_ptr = reinterpret_cast<const uint8_t *>(name.c_str());
    msg.insert(msg.end(), name_ptr, name_ptr + name.size() + 1);
    // node mac, terminated by null byte
    const uint8_t *mac_ptr = reinterpret_cast<const uint8_t *>(mac.c_str());
    msg.insert(msg.end(), mac_ptr, mac_ptr + mac.size() + 1);

    aerr = write_frame_(msg.data(), msg.size());
    if (aerr != APIError::OK)
      return aerr;

    // start handshake
    aerr = init_handshake_();
    if (aerr != APIError::OK)
      return aerr;

    state_ = State::HANDSHAKE;
  }
  if (state_ == State::HANDSHAKE) {
    int action = noise_handshakestate_get_action(handshake_);
    if (action == NOISE_ACTION_READ_MESSAGE) {
      // waiting for handshake msg
      ParsedFrame frame;
      aerr = try_read_frame_(&frame);
      if (aerr == APIError::BAD_INDICATOR) {
        send_explicit_handshake_reject_("Bad indicator byte");
        return aerr;
      }
      if (aerr == APIError::BAD_HANDSHAKE_PACKET_LEN) {
        send_explicit_handshake_reject_("Bad handshake packet len");
        return aerr;
      }
      if (aerr != APIError::OK)
        return aerr;

      if (frame.msg.empty()) {
        send_explicit_handshake_reject_("Empty handshake message");
        return APIError::BAD_HANDSHAKE_ERROR_BYTE;
      } else if (frame.msg[0] != 0x00) {
        HELPER_LOG("Bad handshake error byte: %u", frame.msg[0]);
        send_explicit_handshake_reject_("Bad handshake error byte");
        return APIError::BAD_HANDSHAKE_ERROR_BYTE;
      }

      NoiseBuffer mbuf;
      noise_buffer_init(mbuf);
      noise_buffer_set_input(mbuf, frame.msg.data() + 1, frame.msg.size() - 1);
      err = noise_handshakestate_read_message(handshake_, &mbuf, nullptr);
      if (err != 0) {
        state_ = State::FAILED;
        HELPER_LOG("noise_handshakestate_read_message failed: %s", noise_err_to_str(err).c_str());
        if (err == NOISE_ERROR_MAC_FAILURE) {
          send_explicit_handshake_reject_("Handshake MAC failure");
        } else {
          send_explicit_handshake_reject_("Handshake error");
        }
        return APIError::HANDSHAKESTATE_READ_FAILED;
      }

      aerr = check_handshake_finished_();
      if (aerr != APIError::OK)
        return aerr;
    } else if (action == NOISE_ACTION_WRITE_MESSAGE) {
      uint8_t buffer[65];
      NoiseBuffer mbuf;
      noise_buffer_init(mbuf);
      noise_buffer_set_output(mbuf, buffer + 1, sizeof(buffer) - 1);

      err = noise_handshakestate_write_message(handshake_, &mbuf, nullptr);
      if (err != 0) {
        state_ = State::FAILED;
        HELPER_LOG("noise_handshakestate_write_message failed: %s", noise_err_to_str(err).c_str());
        return APIError::HANDSHAKESTATE_WRITE_FAILED;
      }
      buffer[0] = 0x00;  // success

      aerr = write_frame_(buffer, mbuf.size + 1);
      if (aerr != APIError::OK)
        return aerr;
      aerr = check_handshake_finished_();
      if (aerr != APIError::OK)
        return aerr;
    } else {
      // bad state for action
      state_ = State::FAILED;
      HELPER_LOG("Bad action for handshake: %d", action);
      return APIError::HANDSHAKESTATE_BAD_STATE;
    }
  }
  if (state_ == State::CLOSED || state_ == State::FAILED) {
    return APIError::BAD_STATE;
  }
  return APIError::OK;
}
void APINoiseFrameHelper::send_explicit_handshake_reject_(const std::string &reason) {
  std::vector<uint8_t> data;
  data.resize(reason.length() + 1);
  data[0] = 0x01;  // failure

  // Copy error message in bulk
  if (!reason.empty()) {
    std::memcpy(data.data() + 1, reason.c_str(), reason.length());
  }

  // temporarily remove failed state
  auto orig_state = state_;
  state_ = State::EXPLICIT_REJECT;
  write_frame_(data.data(), data.size());
  state_ = orig_state;
}
APIError APINoiseFrameHelper::read_packet(ReadPacketBuffer *buffer) {
  int err;
  APIError aerr;
  aerr = state_action_();
  if (aerr != APIError::OK) {
    return aerr;
  }

  if (state_ != State::DATA) {
    return APIError::WOULD_BLOCK;
  }

  ParsedFrame frame;
  aerr = try_read_frame_(&frame);
  if (aerr != APIError::OK)
    return aerr;

  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);
  noise_buffer_set_inout(mbuf, frame.msg.data(), frame.msg.size(), frame.msg.size());
  err = noise_cipherstate_decrypt(recv_cipher_, &mbuf);
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("noise_cipherstate_decrypt failed: %s", noise_err_to_str(err).c_str());
    return APIError::CIPHERSTATE_DECRYPT_FAILED;
  }

  uint16_t msg_size = mbuf.size;
  uint8_t *msg_data = frame.msg.data();
  if (msg_size < 4) {
    state_ = State::FAILED;
    HELPER_LOG("Bad data packet: size %d too short", msg_size);
    return APIError::BAD_DATA_PACKET;
  }

  uint16_t type = (((uint16_t) msg_data[0]) << 8) | msg_data[1];
  uint16_t data_len = (((uint16_t) msg_data[2]) << 8) | msg_data[3];
  if (data_len > msg_size - 4) {
    state_ = State::FAILED;
    HELPER_LOG("Bad data packet: data_len %u greater than msg_size %u", data_len, msg_size);
    return APIError::BAD_DATA_PACKET;
  }

  buffer->container = std::move(frame.msg);
  buffer->data_offset = 4;
  buffer->data_len = data_len;
  buffer->type = type;
  return APIError::OK;
}
APIError APINoiseFrameHelper::write_protobuf_packet(uint8_t type, ProtoWriteBuffer buffer) {
  // Resize to include MAC space (required for Noise encryption)
  buffer.get_buffer()->resize(buffer.get_buffer()->size() + frame_footer_size_);
  PacketInfo packet{type, 0,
                    static_cast<uint16_t>(buffer.get_buffer()->size() - frame_header_padding_ - frame_footer_size_)};
  return write_protobuf_packets(buffer, std::span<const PacketInfo>(&packet, 1));
}

APIError APINoiseFrameHelper::write_protobuf_packets(ProtoWriteBuffer buffer, std::span<const PacketInfo> packets) {
  APIError aerr = state_action_();
  if (aerr != APIError::OK) {
    return aerr;
  }

  if (state_ != State::DATA) {
    return APIError::WOULD_BLOCK;
  }

  if (packets.empty()) {
    return APIError::OK;
  }

  std::vector<uint8_t> *raw_buffer = buffer.get_buffer();
  uint8_t *buffer_data = raw_buffer->data();  // Cache buffer pointer

  this->reusable_iovs_.clear();
  this->reusable_iovs_.reserve(packets.size());

  // We need to encrypt each packet in place
  for (const auto &packet : packets) {
    // The buffer already has padding at offset
    uint8_t *buf_start = buffer_data + packet.offset;

    // Write noise header
    buf_start[0] = 0x01;  // indicator
    // buf_start[1], buf_start[2] to be set after encryption

    // Write message header (to be encrypted)
    const uint8_t msg_offset = 3;
    buf_start[msg_offset] = static_cast<uint8_t>(packet.message_type >> 8);      // type high byte
    buf_start[msg_offset + 1] = static_cast<uint8_t>(packet.message_type);       // type low byte
    buf_start[msg_offset + 2] = static_cast<uint8_t>(packet.payload_size >> 8);  // data_len high byte
    buf_start[msg_offset + 3] = static_cast<uint8_t>(packet.payload_size);       // data_len low byte
    // payload data is already in the buffer starting at offset + 7

    // Make sure we have space for MAC
    // The buffer should already have been sized appropriately

    // Encrypt the message in place
    NoiseBuffer mbuf;
    noise_buffer_init(mbuf);
    noise_buffer_set_inout(mbuf, buf_start + msg_offset, 4 + packet.payload_size,
                           4 + packet.payload_size + frame_footer_size_);

    int err = noise_cipherstate_encrypt(send_cipher_, &mbuf);
    if (err != 0) {
      state_ = State::FAILED;
      HELPER_LOG("noise_cipherstate_encrypt failed: %s", noise_err_to_str(err).c_str());
      return APIError::CIPHERSTATE_ENCRYPT_FAILED;
    }

    // Fill in the encrypted size
    buf_start[1] = static_cast<uint8_t>(mbuf.size >> 8);
    buf_start[2] = static_cast<uint8_t>(mbuf.size);

    // Add iovec for this encrypted packet
    this->reusable_iovs_.push_back(
        {buf_start, static_cast<size_t>(3 + mbuf.size)});  // indicator + size + encrypted data
  }

  // Send all encrypted packets in one writev call
  return this->write_raw_(this->reusable_iovs_.data(), this->reusable_iovs_.size());
}

APIError APINoiseFrameHelper::write_frame_(const uint8_t *data, uint16_t len) {
  uint8_t header[3];
  header[0] = 0x01;  // indicator
  header[1] = (uint8_t) (len >> 8);
  header[2] = (uint8_t) len;

  struct iovec iov[2];
  iov[0].iov_base = header;
  iov[0].iov_len = 3;
  if (len == 0) {
    return this->write_raw_(iov, 1);
  }
  iov[1].iov_base = const_cast<uint8_t *>(data);
  iov[1].iov_len = len;

  return this->write_raw_(iov, 2);
}

/** Initiate the data structures for the handshake.
 *
 * @return 0 on success, -1 on error (check errno)
 */
APIError APINoiseFrameHelper::init_handshake_() {
  int err;
  memset(&nid_, 0, sizeof(nid_));
  // const char *proto = "Noise_NNpsk0_25519_ChaChaPoly_SHA256";
  // err = noise_protocol_name_to_id(&nid_, proto, strlen(proto));
  nid_.pattern_id = NOISE_PATTERN_NN;
  nid_.cipher_id = NOISE_CIPHER_CHACHAPOLY;
  nid_.dh_id = NOISE_DH_CURVE25519;
  nid_.prefix_id = NOISE_PREFIX_STANDARD;
  nid_.hybrid_id = NOISE_DH_NONE;
  nid_.hash_id = NOISE_HASH_SHA256;
  nid_.modifier_ids[0] = NOISE_MODIFIER_PSK0;

  err = noise_handshakestate_new_by_id(&handshake_, &nid_, NOISE_ROLE_RESPONDER);
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("noise_handshakestate_new_by_id failed: %s", noise_err_to_str(err).c_str());
    return APIError::HANDSHAKESTATE_SETUP_FAILED;
  }

  const auto &psk = ctx_->get_psk();
  err = noise_handshakestate_set_pre_shared_key(handshake_, psk.data(), psk.size());
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("noise_handshakestate_set_pre_shared_key failed: %s", noise_err_to_str(err).c_str());
    return APIError::HANDSHAKESTATE_SETUP_FAILED;
  }

  err = noise_handshakestate_set_prologue(handshake_, prologue_.data(), prologue_.size());
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("noise_handshakestate_set_prologue failed: %s", noise_err_to_str(err).c_str());
    return APIError::HANDSHAKESTATE_SETUP_FAILED;
  }
  // set_prologue copies it into handshakestate, so we can get rid of it now
  prologue_ = {};

  err = noise_handshakestate_start(handshake_);
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("noise_handshakestate_start failed: %s", noise_err_to_str(err).c_str());
    return APIError::HANDSHAKESTATE_SETUP_FAILED;
  }
  return APIError::OK;
}

APIError APINoiseFrameHelper::check_handshake_finished_() {
  assert(state_ == State::HANDSHAKE);

  int action = noise_handshakestate_get_action(handshake_);
  if (action == NOISE_ACTION_READ_MESSAGE || action == NOISE_ACTION_WRITE_MESSAGE)
    return APIError::OK;
  if (action != NOISE_ACTION_SPLIT) {
    state_ = State::FAILED;
    HELPER_LOG("Bad action for handshake: %d", action);
    return APIError::HANDSHAKESTATE_BAD_STATE;
  }
  int err = noise_handshakestate_split(handshake_, &send_cipher_, &recv_cipher_);
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("noise_handshakestate_split failed: %s", noise_err_to_str(err).c_str());
    return APIError::HANDSHAKESTATE_SPLIT_FAILED;
  }

  frame_footer_size_ = noise_cipherstate_get_mac_length(send_cipher_);

  HELPER_LOG("Handshake complete!");
  noise_handshakestate_free(handshake_);
  handshake_ = nullptr;
  state_ = State::DATA;
  return APIError::OK;
}

APINoiseFrameHelper::~APINoiseFrameHelper() {
  if (handshake_ != nullptr) {
    noise_handshakestate_free(handshake_);
    handshake_ = nullptr;
  }
  if (send_cipher_ != nullptr) {
    noise_cipherstate_free(send_cipher_);
    send_cipher_ = nullptr;
  }
  if (recv_cipher_ != nullptr) {
    noise_cipherstate_free(recv_cipher_);
    recv_cipher_ = nullptr;
  }
}

extern "C" {
// declare how noise generates random bytes (here with a good HWRNG based on the RF system)
void noise_rand_bytes(void *output, size_t len) {
  if (!esphome::random_bytes(reinterpret_cast<uint8_t *>(output), len)) {
    ESP_LOGE(TAG, "Acquiring random bytes failed; rebooting");
    arch_restart();
  }
}
}

#endif  // USE_API_NOISE

#ifdef USE_API_PLAINTEXT

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

/** Read a packet into the rx_buf_. If successful, stores frame data in the frame parameter
 *
 * @param frame: The struct to hold the frame information in.
 *   msg: store the parsed frame in that struct
 *
 * @return See APIError
 *
 * error API_ERROR_BAD_INDICATOR: Bad indicator byte at start of frame.
 */
APIError APIPlaintextFrameHelper::try_read_frame_(ParsedFrame *frame) {
  if (frame == nullptr) {
    HELPER_LOG("Bad argument for try_read_frame_");
    return APIError::BAD_ARG;
  }

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

    if (msg_size_varint->as_uint32() > std::numeric_limits<uint16_t>::max()) {
      state_ = State::FAILED;
      HELPER_LOG("Bad packet: message size %" PRIu32 " exceeds maximum %u", msg_size_varint->as_uint32(),
                 std::numeric_limits<uint16_t>::max());
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

  // reserve space for body
  if (rx_buf_.size() != rx_header_parsed_len_) {
    rx_buf_.resize(rx_header_parsed_len_);
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

  // uncomment for even more debugging
#ifdef HELPER_LOG_PACKETS
  ESP_LOGVV(TAG, "Received frame: %s", format_hex_pretty(rx_buf_).c_str());
#endif
  frame->msg = std::move(rx_buf_);
  // consume msg
  rx_buf_ = {};
  rx_buf_len_ = 0;
  rx_header_buf_pos_ = 0;
  rx_header_parsed_ = false;
  return APIError::OK;
}
APIError APIPlaintextFrameHelper::read_packet(ReadPacketBuffer *buffer) {
  APIError aerr;

  if (state_ != State::DATA) {
    return APIError::WOULD_BLOCK;
  }

  ParsedFrame frame;
  aerr = try_read_frame_(&frame);
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
      const char msg[] = "\x00"
                         "Bad indicator byte";
      iov[0].iov_base = (void *) msg;
      iov[0].iov_len = 19;
      this->write_raw_(iov, 1);
    }
    return aerr;
  }

  buffer->container = std::move(frame.msg);
  buffer->data_offset = 0;
  buffer->data_len = rx_header_parsed_len_;
  buffer->type = rx_header_parsed_type_;
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

  std::vector<uint8_t> *raw_buffer = buffer.get_buffer();
  uint8_t *buffer_data = raw_buffer->data();  // Cache buffer pointer

  this->reusable_iovs_.clear();
  this->reusable_iovs_.reserve(packets.size());

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
    this->reusable_iovs_.push_back(
        {buf_start + header_offset, static_cast<size_t>(total_header_len + packet.payload_size)});
  }

  // Send all packets in one writev call
  return write_raw_(this->reusable_iovs_.data(), this->reusable_iovs_.size());
}

#endif  // USE_API_PLAINTEXT

}  // namespace api
}  // namespace esphome
#endif
