#include "api_frame_helper.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "proto.h"
#include <cstring>

namespace esphome {
namespace api {

static const char *const TAG = "api.socket";

/// Is the given return value (from write syscalls) a wouldblock error?
bool is_would_block(ssize_t ret) {
  if (ret == -1) {
    return errno == EWOULDBLOCK || errno == EAGAIN;
  }
  return ret == 0;
}

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

#define HELPER_LOG(msg, ...) ESP_LOGVV(TAG, "%s: " msg, info_.c_str(), ##__VA_ARGS__)
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
  if (state_ != State::INITIALIZE || socket_ == nullptr) {
    HELPER_LOG("Bad state for init %d", (int) state_);
    return APIError::BAD_STATE;
  }
  int err = socket_->setblocking(false);
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("Setting nonblocking failed with errno %d", errno);
    return APIError::TCP_NONBLOCKING_FAILED;
  }

  int enable = 1;
  err = socket_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("Setting nodelay failed with errno %d", errno);
    return APIError::TCP_NODELAY_FAILED;
  }

  // init prologue
  prologue_.insert(prologue_.end(), PROLOGUE_INIT, PROLOGUE_INIT + strlen(PROLOGUE_INIT));

  state_ = State::CLIENT_HELLO;
  return APIError::OK;
}
/// Run through handshake messages (if in that phase)
APIError APINoiseFrameHelper::loop() {
  APIError err = state_action_();
  if (err == APIError::WOULD_BLOCK)
    return APIError::OK;
  if (err != APIError::OK)
    return err;
  if (!tx_buf_.empty()) {
    err = try_send_tx_buf_();
    if (err != APIError::OK) {
      return err;
    }
  }
  return APIError::OK;
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
    size_t to_read = 3 - rx_header_buf_len_;
    ssize_t received = socket_->read(&rx_header_buf_[rx_header_buf_len_], to_read);
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
    rx_header_buf_len_ += received;
    if ((size_t) received != to_read) {
      // not a full read
      return APIError::WOULD_BLOCK;
    }

    // header reading done
  }

  // read body
  uint8_t indicator = rx_header_buf_[0];
  if (indicator != 0x01) {
    state_ = State::FAILED;
    HELPER_LOG("Bad indicator byte %u", indicator);
    return APIError::BAD_INDICATOR;
  }

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
    size_t to_read = msg_size - rx_buf_len_;
    ssize_t received = socket_->read(&rx_buf_[rx_buf_len_], to_read);
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
    rx_buf_len_ += received;
    if ((size_t) received != to_read) {
      // not all read
      return APIError::WOULD_BLOCK;
    }
  }

  // uncomment for even more debugging
#ifdef HELPER_LOG_PACKETS
  ESP_LOGVV(TAG, "Received frame: %s", hexencode(rx_buf_).c_str());
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
 * If an error occured, returns that error. Only returns OK if the transport is ready for data
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
    prologue_.push_back((uint8_t)(frame.msg.size() >> 8));
    prologue_.push_back((uint8_t) frame.msg.size());
    prologue_.insert(prologue_.end(), frame.msg.begin(), frame.msg.end());

    state_ = State::SERVER_HELLO;
  }
  if (state_ == State::SERVER_HELLO) {
    // send server hello
    uint8_t msg[1];
    msg[0] = 0x01;  // chosen proto
    aerr = write_frame_(msg, 1);
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
  for (size_t i = 0; i < reason.length(); i++) {
    data[i + 1] = (uint8_t) reason[i];
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

  size_t msg_size = mbuf.size;
  uint8_t *msg_data = frame.msg.data();
  if (msg_size < 4) {
    state_ = State::FAILED;
    HELPER_LOG("Bad data packet: size %d too short", msg_size);
    return APIError::BAD_DATA_PACKET;
  }

  // uint16_t type;
  // uint16_t data_len;
  // uint8_t *data;
  // uint8_t *padding;  zero or more bytes to fill up the rest of the packet
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
bool APINoiseFrameHelper::can_write_without_blocking() { return state_ == State::DATA && tx_buf_.empty(); }
APIError APINoiseFrameHelper::write_packet(uint16_t type, const uint8_t *payload, size_t payload_len) {
  int err;
  APIError aerr;
  aerr = state_action_();
  if (aerr != APIError::OK) {
    return aerr;
  }

  if (state_ != State::DATA) {
    return APIError::WOULD_BLOCK;
  }

  size_t padding = 0;
  size_t msg_len = 4 + payload_len + padding;
  size_t frame_len = 3 + msg_len + noise_cipherstate_get_mac_length(send_cipher_);
  auto tmpbuf = std::unique_ptr<uint8_t[]>{new (std::nothrow) uint8_t[frame_len]};
  if (tmpbuf == nullptr) {
    HELPER_LOG("Could not allocate for writing packet");
    return APIError::OUT_OF_MEMORY;
  }

  tmpbuf[0] = 0x01;  // indicator
  // tmpbuf[1], tmpbuf[2] to be set later
  const uint8_t msg_offset = 3;
  const uint8_t payload_offset = msg_offset + 4;
  tmpbuf[msg_offset + 0] = (uint8_t)(type >> 8);  // type
  tmpbuf[msg_offset + 1] = (uint8_t) type;
  tmpbuf[msg_offset + 2] = (uint8_t)(payload_len >> 8);  // data_len
  tmpbuf[msg_offset + 3] = (uint8_t) payload_len;
  // copy data
  std::copy(payload, payload + payload_len, &tmpbuf[payload_offset]);
  // fill padding with zeros
  std::fill(&tmpbuf[payload_offset + payload_len], &tmpbuf[frame_len], 0);

  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);
  noise_buffer_set_inout(mbuf, &tmpbuf[msg_offset], msg_len, frame_len - msg_offset);
  err = noise_cipherstate_encrypt(send_cipher_, &mbuf);
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("noise_cipherstate_encrypt failed: %s", noise_err_to_str(err).c_str());
    return APIError::CIPHERSTATE_ENCRYPT_FAILED;
  }

  size_t total_len = 3 + mbuf.size;
  tmpbuf[1] = (uint8_t)(mbuf.size >> 8);
  tmpbuf[2] = (uint8_t) mbuf.size;

  struct iovec iov;
  iov.iov_base = &tmpbuf[0];
  iov.iov_len = total_len;

  // write raw to not have two packets sent if NAGLE disabled
  return write_raw_(&iov, 1);
}
APIError APINoiseFrameHelper::try_send_tx_buf_() {
  // try send from tx_buf
  while (state_ != State::CLOSED && !tx_buf_.empty()) {
    ssize_t sent = socket_->write(tx_buf_.data(), tx_buf_.size());
    if (sent == -1) {
      if (errno == EWOULDBLOCK || errno == EAGAIN)
        break;
      state_ = State::FAILED;
      HELPER_LOG("Socket write failed with errno %d", errno);
      return APIError::SOCKET_WRITE_FAILED;
    } else if (sent == 0) {
      break;
    }
    // TODO: inefficient if multiple packets in txbuf
    // replace with deque of buffers
    tx_buf_.erase(tx_buf_.begin(), tx_buf_.begin() + sent);
  }

  return APIError::OK;
}
/** Write the data to the socket, or buffer it a write would block
 *
 * @param data The data to write
 * @param len The length of data
 */
APIError APINoiseFrameHelper::write_raw_(const struct iovec *iov, int iovcnt) {
  if (iovcnt == 0)
    return APIError::OK;
  APIError aerr;

  size_t total_write_len = 0;
  for (int i = 0; i < iovcnt; i++) {
#ifdef HELPER_LOG_PACKETS
    ESP_LOGVV(TAG, "Sending raw: %s", hexencode(reinterpret_cast<uint8_t *>(iov[i].iov_base), iov[i].iov_len).c_str());
#endif
    total_write_len += iov[i].iov_len;
  }

  if (!tx_buf_.empty()) {
    // try to empty tx_buf_ first
    aerr = try_send_tx_buf_();
    if (aerr != APIError::OK && aerr != APIError::WOULD_BLOCK)
      return aerr;
  }

  if (!tx_buf_.empty()) {
    // tx buf not empty, can't write now because then stream would be inconsistent
    for (int i = 0; i < iovcnt; i++) {
      tx_buf_.insert(tx_buf_.end(), reinterpret_cast<uint8_t *>(iov[i].iov_base),
                     reinterpret_cast<uint8_t *>(iov[i].iov_base) + iov[i].iov_len);
    }
    return APIError::OK;
  }

  ssize_t sent = socket_->writev(iov, iovcnt);
  if (is_would_block(sent)) {
    // operation would block, add buffer to tx_buf
    for (int i = 0; i < iovcnt; i++) {
      tx_buf_.insert(tx_buf_.end(), reinterpret_cast<uint8_t *>(iov[i].iov_base),
                     reinterpret_cast<uint8_t *>(iov[i].iov_base) + iov[i].iov_len);
    }
    return APIError::OK;
  } else if (sent == -1) {
    // an error occured
    state_ = State::FAILED;
    HELPER_LOG("Socket write failed with errno %d", errno);
    return APIError::SOCKET_WRITE_FAILED;
  } else if ((size_t) sent != total_write_len) {
    // partially sent, add end to tx_buf
    size_t to_consume = sent;
    for (int i = 0; i < iovcnt; i++) {
      if (to_consume >= iov[i].iov_len) {
        to_consume -= iov[i].iov_len;
      } else {
        tx_buf_.insert(tx_buf_.end(), reinterpret_cast<uint8_t *>(iov[i].iov_base) + to_consume,
                       reinterpret_cast<uint8_t *>(iov[i].iov_base) + iov[i].iov_len);
        to_consume = 0;
      }
    }
    return APIError::OK;
  }
  // fully sent
  return APIError::OK;
}
APIError APINoiseFrameHelper::write_frame_(const uint8_t *data, size_t len) {
  uint8_t header[3];
  header[0] = 0x01;  // indicator
  header[1] = (uint8_t)(len >> 8);
  header[2] = (uint8_t) len;

  struct iovec iov[2];
  iov[0].iov_base = header;
  iov[0].iov_len = 3;
  iov[1].iov_base = const_cast<uint8_t *>(data);
  iov[1].iov_len = len;

  return write_raw_(iov, 2);
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

APIError APINoiseFrameHelper::close() {
  state_ = State::CLOSED;
  int err = socket_->close();
  if (err == -1)
    return APIError::CLOSE_FAILED;
  return APIError::OK;
}
APIError APINoiseFrameHelper::shutdown(int how) {
  int err = socket_->shutdown(how);
  if (err == -1)
    return APIError::SHUTDOWN_FAILED;
  if (how == SHUT_RDWR) {
    state_ = State::CLOSED;
  }
  return APIError::OK;
}
extern "C" {
// declare how noise generates random bytes (here with a good HWRNG based on the RF system)
void noise_rand_bytes(void *output, size_t len) { esphome::fill_random(reinterpret_cast<uint8_t *>(output), len); }
}
#endif  // USE_API_NOISE

#ifdef USE_API_PLAINTEXT

/// Initialize the frame helper, returns OK if successful.
APIError APIPlaintextFrameHelper::init() {
  if (state_ != State::INITIALIZE || socket_ == nullptr) {
    HELPER_LOG("Bad state for init %d", (int) state_);
    return APIError::BAD_STATE;
  }
  int err = socket_->setblocking(false);
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("Setting nonblocking failed with errno %d", errno);
    return APIError::TCP_NONBLOCKING_FAILED;
  }
  int enable = 1;
  err = socket_->setsockopt(IPPROTO_TCP, TCP_NODELAY, &enable, sizeof(int));
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("Setting nodelay failed with errno %d", errno);
    return APIError::TCP_NODELAY_FAILED;
  }

  state_ = State::DATA;
  return APIError::OK;
}
/// Not used for plaintext
APIError APIPlaintextFrameHelper::loop() {
  if (state_ != State::DATA) {
    return APIError::BAD_STATE;
  }
  // try send pending TX data
  if (!tx_buf_.empty()) {
    APIError err = try_send_tx_buf_();
    if (err != APIError::OK) {
      return err;
    }
  }
  return APIError::OK;
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
    uint8_t data;
    ssize_t received = socket_->read(&data, 1);
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
    rx_header_buf_.push_back(data);

    // try parse header
    if (rx_header_buf_[0] != 0x00) {
      state_ = State::FAILED;
      HELPER_LOG("Bad indicator byte %u", rx_header_buf_[0]);
      return APIError::BAD_INDICATOR;
    }

    size_t i = 1;
    uint32_t consumed = 0;
    auto msg_size_varint = ProtoVarInt::parse(&rx_header_buf_[i], rx_header_buf_.size() - i, &consumed);
    if (!msg_size_varint.has_value()) {
      // not enough data there yet
      continue;
    }

    i += consumed;
    rx_header_parsed_len_ = msg_size_varint->as_uint32();

    auto msg_type_varint = ProtoVarInt::parse(&rx_header_buf_[i], rx_header_buf_.size() - i, &consumed);
    if (!msg_type_varint.has_value()) {
      // not enough data there yet
      continue;
    }
    rx_header_parsed_type_ = msg_type_varint->as_uint32();
    rx_header_parsed_ = true;
  }
  // header reading done

  // reserve space for body
  if (rx_buf_.size() != rx_header_parsed_len_) {
    rx_buf_.resize(rx_header_parsed_len_);
  }

  if (rx_buf_len_ < rx_header_parsed_len_) {
    // more data to read
    size_t to_read = rx_header_parsed_len_ - rx_buf_len_;
    ssize_t received = socket_->read(&rx_buf_[rx_buf_len_], to_read);
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
    rx_buf_len_ += received;
    if ((size_t) received != to_read) {
      // not all read
      return APIError::WOULD_BLOCK;
    }
  }

  // uncomment for even more debugging
#ifdef HELPER_LOG_PACKETS
  ESP_LOGVV(TAG, "Received frame: %s", hexencode(rx_buf_).c_str());
#endif
  frame->msg = std::move(rx_buf_);
  // consume msg
  rx_buf_ = {};
  rx_buf_len_ = 0;
  rx_header_buf_.clear();
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
  if (aerr != APIError::OK)
    return aerr;

  buffer->container = std::move(frame.msg);
  buffer->data_offset = 0;
  buffer->data_len = rx_header_parsed_len_;
  buffer->type = rx_header_parsed_type_;
  return APIError::OK;
}
bool APIPlaintextFrameHelper::can_write_without_blocking() { return state_ == State::DATA && tx_buf_.empty(); }
APIError APIPlaintextFrameHelper::write_packet(uint16_t type, const uint8_t *payload, size_t payload_len) {
  if (state_ != State::DATA) {
    return APIError::BAD_STATE;
  }

  std::vector<uint8_t> header;
  header.push_back(0x00);
  ProtoVarInt(payload_len).encode(header);
  ProtoVarInt(type).encode(header);

  struct iovec iov[2];
  iov[0].iov_base = &header[0];
  iov[0].iov_len = header.size();
  iov[1].iov_base = const_cast<uint8_t *>(payload);
  iov[1].iov_len = payload_len;

  return write_raw_(iov, 2);
}
APIError APIPlaintextFrameHelper::try_send_tx_buf_() {
  // try send from tx_buf
  while (state_ != State::CLOSED && !tx_buf_.empty()) {
    ssize_t sent = socket_->write(tx_buf_.data(), tx_buf_.size());
    if (is_would_block(sent)) {
      break;
    } else if (sent == -1) {
      state_ = State::FAILED;
      HELPER_LOG("Socket write failed with errno %d", errno);
      return APIError::SOCKET_WRITE_FAILED;
    }
    // TODO: inefficient if multiple packets in txbuf
    // replace with deque of buffers
    tx_buf_.erase(tx_buf_.begin(), tx_buf_.begin() + sent);
  }

  return APIError::OK;
}
/** Write the data to the socket, or buffer it a write would block
 *
 * @param data The data to write
 * @param len The length of data
 */
APIError APIPlaintextFrameHelper::write_raw_(const struct iovec *iov, int iovcnt) {
  if (iovcnt == 0)
    return APIError::OK;
  APIError aerr;

  size_t total_write_len = 0;
  for (int i = 0; i < iovcnt; i++) {
#ifdef HELPER_LOG_PACKETS
    ESP_LOGVV(TAG, "Sending raw: %s", hexencode(reinterpret_cast<uint8_t *>(iov[i].iov_base), iov[i].iov_len).c_str());
#endif
    total_write_len += iov[i].iov_len;
  }

  if (!tx_buf_.empty()) {
    // try to empty tx_buf_ first
    aerr = try_send_tx_buf_();
    if (aerr != APIError::OK && aerr != APIError::WOULD_BLOCK)
      return aerr;
  }

  if (!tx_buf_.empty()) {
    // tx buf not empty, can't write now because then stream would be inconsistent
    for (int i = 0; i < iovcnt; i++) {
      tx_buf_.insert(tx_buf_.end(), reinterpret_cast<uint8_t *>(iov[i].iov_base),
                     reinterpret_cast<uint8_t *>(iov[i].iov_base) + iov[i].iov_len);
    }
    return APIError::OK;
  }

  ssize_t sent = socket_->writev(iov, iovcnt);
  if (is_would_block(sent)) {
    // operation would block, add buffer to tx_buf
    for (int i = 0; i < iovcnt; i++) {
      tx_buf_.insert(tx_buf_.end(), reinterpret_cast<uint8_t *>(iov[i].iov_base),
                     reinterpret_cast<uint8_t *>(iov[i].iov_base) + iov[i].iov_len);
    }
    return APIError::OK;
  } else if (sent == -1) {
    // an error occured
    state_ = State::FAILED;
    HELPER_LOG("Socket write failed with errno %d", errno);
    return APIError::SOCKET_WRITE_FAILED;
  } else if ((size_t) sent != total_write_len) {
    // partially sent, add end to tx_buf
    size_t to_consume = sent;
    for (int i = 0; i < iovcnt; i++) {
      if (to_consume >= iov[i].iov_len) {
        to_consume -= iov[i].iov_len;
      } else {
        tx_buf_.insert(tx_buf_.end(), reinterpret_cast<uint8_t *>(iov[i].iov_base) + to_consume,
                       reinterpret_cast<uint8_t *>(iov[i].iov_base) + iov[i].iov_len);
        to_consume = 0;
      }
    }
    return APIError::OK;
  }
  // fully sent
  return APIError::OK;
}

APIError APIPlaintextFrameHelper::close() {
  state_ = State::CLOSED;
  int err = socket_->close();
  if (err == -1)
    return APIError::CLOSE_FAILED;
  return APIError::OK;
}
APIError APIPlaintextFrameHelper::shutdown(int how) {
  int err = socket_->shutdown(how);
  if (err == -1)
    return APIError::SHUTDOWN_FAILED;
  if (how == SHUT_RDWR) {
    state_ = State::CLOSED;
  }
  return APIError::OK;
}
#endif  // USE_API_PLAINTEXT

}  // namespace api
}  // namespace esphome
