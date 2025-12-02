#include "api_frame_helper_noise.h"
#ifdef USE_API
#ifdef USE_API_NOISE
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

static const char *const TAG = "api.noise";
#ifdef USE_ESP8266
static const char PROLOGUE_INIT[] PROGMEM = "NoiseAPIInit";
#else
static const char *const PROLOGUE_INIT = "NoiseAPIInit";
#endif
static constexpr size_t PROLOGUE_INIT_LEN = 12;  // strlen("NoiseAPIInit")

#define HELPER_LOG(msg, ...) \
  ESP_LOGVV(TAG, "%s (%s): " msg, this->client_info_->name.c_str(), this->client_info_->peername.c_str(), ##__VA_ARGS__)

#ifdef HELPER_LOG_PACKETS
#define LOG_PACKET_RECEIVED(buffer) ESP_LOGVV(TAG, "Received frame: %s", format_hex_pretty(buffer).c_str())
#define LOG_PACKET_SENDING(data, len) ESP_LOGVV(TAG, "Sending raw: %s", format_hex_pretty(data, len).c_str())
#else
#define LOG_PACKET_RECEIVED(buffer) ((void) 0)
#define LOG_PACKET_SENDING(data, len) ((void) 0)
#endif

/// Convert a noise error code to a readable error
const LogString *noise_err_to_logstr(int err) {
  if (err == NOISE_ERROR_NO_MEMORY)
    return LOG_STR("NO_MEMORY");
  if (err == NOISE_ERROR_UNKNOWN_ID)
    return LOG_STR("UNKNOWN_ID");
  if (err == NOISE_ERROR_UNKNOWN_NAME)
    return LOG_STR("UNKNOWN_NAME");
  if (err == NOISE_ERROR_MAC_FAILURE)
    return LOG_STR("MAC_FAILURE");
  if (err == NOISE_ERROR_NOT_APPLICABLE)
    return LOG_STR("NOT_APPLICABLE");
  if (err == NOISE_ERROR_SYSTEM)
    return LOG_STR("SYSTEM");
  if (err == NOISE_ERROR_REMOTE_KEY_REQUIRED)
    return LOG_STR("REMOTE_KEY_REQUIRED");
  if (err == NOISE_ERROR_LOCAL_KEY_REQUIRED)
    return LOG_STR("LOCAL_KEY_REQUIRED");
  if (err == NOISE_ERROR_PSK_REQUIRED)
    return LOG_STR("PSK_REQUIRED");
  if (err == NOISE_ERROR_INVALID_LENGTH)
    return LOG_STR("INVALID_LENGTH");
  if (err == NOISE_ERROR_INVALID_PARAM)
    return LOG_STR("INVALID_PARAM");
  if (err == NOISE_ERROR_INVALID_STATE)
    return LOG_STR("INVALID_STATE");
  if (err == NOISE_ERROR_INVALID_NONCE)
    return LOG_STR("INVALID_NONCE");
  if (err == NOISE_ERROR_INVALID_PRIVATE_KEY)
    return LOG_STR("INVALID_PRIVATE_KEY");
  if (err == NOISE_ERROR_INVALID_PUBLIC_KEY)
    return LOG_STR("INVALID_PUBLIC_KEY");
  if (err == NOISE_ERROR_INVALID_FORMAT)
    return LOG_STR("INVALID_FORMAT");
  if (err == NOISE_ERROR_INVALID_SIGNATURE)
    return LOG_STR("INVALID_SIGNATURE");
  return LOG_STR("UNKNOWN");
}

/// Initialize the frame helper, returns OK if successful.
APIError APINoiseFrameHelper::init() {
  APIError err = init_common_();
  if (err != APIError::OK) {
    return err;
  }

  // init prologue
  size_t old_size = prologue_.size();
  prologue_.resize(old_size + PROLOGUE_INIT_LEN);
#ifdef USE_ESP8266
  memcpy_P(prologue_.data() + old_size, PROLOGUE_INIT, PROLOGUE_INIT_LEN);
#else
  std::memcpy(prologue_.data() + old_size, PROLOGUE_INIT, PROLOGUE_INIT_LEN);
#endif

  state_ = State::CLIENT_HELLO;
  return APIError::OK;
}
// Helper for handling handshake frame errors
APIError APINoiseFrameHelper::handle_handshake_frame_error_(APIError aerr) {
  if (aerr == APIError::BAD_INDICATOR) {
    send_explicit_handshake_reject_(LOG_STR("Bad indicator byte"));
  } else if (aerr == APIError::BAD_HANDSHAKE_PACKET_LEN) {
    send_explicit_handshake_reject_(LOG_STR("Bad handshake packet len"));
  }
  return aerr;
}

// Helper for handling noise library errors
APIError APINoiseFrameHelper::handle_noise_error_(int err, const LogString *func_name, APIError api_err) {
  if (err != 0) {
    state_ = State::FAILED;
    HELPER_LOG("%s failed: %s", LOG_STR_ARG(func_name), LOG_STR_ARG(noise_err_to_logstr(err)));
    return api_err;
  }
  return APIError::OK;
}

/// Run through handshake messages (if in that phase)
APIError APINoiseFrameHelper::loop() {
  // During handshake phase, process as many actions as possible until we can't progress
  // socket_->ready() stays true until next main loop, but state_action() will return
  // WOULD_BLOCK when no more data is available to read
  while (state_ != State::DATA && this->socket_->ready()) {
    APIError err = state_action_();
    if (err == APIError::WOULD_BLOCK) {
      break;
    }
    if (err != APIError::OK) {
      return err;
    }
  }

  // Use base class implementation for buffer sending
  return APIFrameHelper::loop();
}

/** Read a packet into the rx_buf_.
 *
 * @return APIError::OK if a full packet is in rx_buf_
 *
 * errno EWOULDBLOCK: Packet could not be read without blocking. Try again later.
 * errno ENOMEM: Not enough memory for reading packet.
 * errno API_ERROR_BAD_INDICATOR: Bad indicator byte at start of frame.
 * errno API_ERROR_HANDSHAKE_PACKET_LEN: Packet too big for this phase.
 */
APIError APINoiseFrameHelper::try_read_frame_() {
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

  // Check against size limits to prevent OOM: MAX_HANDSHAKE_SIZE for handshake, MAX_MESSAGE_SIZE for data
  uint16_t limit = (state_ == State::DATA) ? MAX_MESSAGE_SIZE : MAX_HANDSHAKE_SIZE;
  if (msg_size > limit) {
    state_ = State::FAILED;
    HELPER_LOG("Bad packet: message size %u exceeds maximum %u", msg_size, limit);
    return (state_ == State::DATA) ? APIError::BAD_DATA_PACKET : APIError::BAD_HANDSHAKE_PACKET_LEN;
  }

  // Reserve space for body
  if (this->rx_buf_.size() != msg_size) {
    this->rx_buf_.resize(msg_size);
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

  LOG_PACKET_RECEIVED(this->rx_buf_);

  // Clear state for next frame (rx_buf_ still contains data for caller)
  this->rx_buf_len_ = 0;
  this->rx_header_buf_len_ = 0;

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
    aerr = this->try_read_frame_();
    if (aerr != APIError::OK) {
      return handle_handshake_frame_error_(aerr);
    }
    // ignore contents, may be used in future for flags
    // Resize for: existing prologue + 2 size bytes + frame data
    size_t old_size = this->prologue_.size();
    this->prologue_.resize(old_size + 2 + this->rx_buf_.size());
    this->prologue_[old_size] = (uint8_t) (this->rx_buf_.size() >> 8);
    this->prologue_[old_size + 1] = (uint8_t) this->rx_buf_.size();
    std::memcpy(this->prologue_.data() + old_size + 2, this->rx_buf_.data(), this->rx_buf_.size());

    state_ = State::SERVER_HELLO;
  }
  if (state_ == State::SERVER_HELLO) {
    // send server hello
    constexpr size_t mac_len = 13;  // 12 hex chars + null terminator
    const std::string &name = App.get_name();
    char mac[mac_len];
    get_mac_address_into_buffer(mac);

    // Calculate positions and sizes
    size_t name_len = name.size() + 1;  // including null terminator
    size_t name_offset = 1;
    size_t mac_offset = name_offset + name_len;
    size_t total_size = 1 + name_len + mac_len;

    auto msg = std::make_unique<uint8_t[]>(total_size);

    // chosen proto
    msg[0] = 0x01;

    // node name, terminated by null byte
    std::memcpy(msg.get() + name_offset, name.c_str(), name_len);
    // node mac, terminated by null byte
    std::memcpy(msg.get() + mac_offset, mac, mac_len);

    aerr = write_frame_(msg.get(), total_size);
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
      aerr = this->try_read_frame_();
      if (aerr != APIError::OK) {
        return handle_handshake_frame_error_(aerr);
      }

      if (this->rx_buf_.empty()) {
        send_explicit_handshake_reject_(LOG_STR("Empty handshake message"));
        return APIError::BAD_HANDSHAKE_ERROR_BYTE;
      } else if (this->rx_buf_[0] != 0x00) {
        HELPER_LOG("Bad handshake error byte: %u", this->rx_buf_[0]);
        send_explicit_handshake_reject_(LOG_STR("Bad handshake error byte"));
        return APIError::BAD_HANDSHAKE_ERROR_BYTE;
      }

      NoiseBuffer mbuf;
      noise_buffer_init(mbuf);
      noise_buffer_set_input(mbuf, this->rx_buf_.data() + 1, this->rx_buf_.size() - 1);
      err = noise_handshakestate_read_message(handshake_, &mbuf, nullptr);
      if (err != 0) {
        // Special handling for MAC failure
        send_explicit_handshake_reject_(err == NOISE_ERROR_MAC_FAILURE ? LOG_STR("Handshake MAC failure")
                                                                       : LOG_STR("Handshake error"));
        return handle_noise_error_(err, LOG_STR("noise_handshakestate_read_message"),
                                   APIError::HANDSHAKESTATE_READ_FAILED);
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
      APIError aerr_write = handle_noise_error_(err, LOG_STR("noise_handshakestate_write_message"),
                                                APIError::HANDSHAKESTATE_WRITE_FAILED);
      if (aerr_write != APIError::OK)
        return aerr_write;
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
void APINoiseFrameHelper::send_explicit_handshake_reject_(const LogString *reason) {
#ifdef USE_STORE_LOG_STR_IN_FLASH
  // On ESP8266 with flash strings, we need to use PROGMEM-aware functions
  size_t reason_len = strlen_P(reinterpret_cast<PGM_P>(reason));
  size_t data_size = reason_len + 1;
  auto data = std::make_unique<uint8_t[]>(data_size);
  data[0] = 0x01;  // failure

  // Copy error message from PROGMEM
  if (reason_len > 0) {
    memcpy_P(data.get() + 1, reinterpret_cast<PGM_P>(reason), reason_len);
  }
#else
  // Normal memory access
  const char *reason_str = LOG_STR_ARG(reason);
  size_t reason_len = strlen(reason_str);
  size_t data_size = reason_len + 1;
  auto data = std::make_unique<uint8_t[]>(data_size);
  data[0] = 0x01;  // failure

  // Copy error message in bulk
  if (reason_len > 0) {
    std::memcpy(data.get() + 1, reason_str, reason_len);
  }
#endif

  // temporarily remove failed state
  auto orig_state = state_;
  state_ = State::EXPLICIT_REJECT;
  write_frame_(data.get(), data_size);
  state_ = orig_state;
}
APIError APINoiseFrameHelper::read_packet(ReadPacketBuffer *buffer) {
  APIError aerr = this->state_action_();
  if (aerr != APIError::OK) {
    return aerr;
  }

  if (this->state_ != State::DATA) {
    return APIError::WOULD_BLOCK;
  }

  aerr = this->try_read_frame_();
  if (aerr != APIError::OK)
    return aerr;

  NoiseBuffer mbuf;
  noise_buffer_init(mbuf);
  noise_buffer_set_inout(mbuf, this->rx_buf_.data(), this->rx_buf_.size(), this->rx_buf_.size());
  int err = noise_cipherstate_decrypt(this->recv_cipher_, &mbuf);
  APIError decrypt_err =
      handle_noise_error_(err, LOG_STR("noise_cipherstate_decrypt"), APIError::CIPHERSTATE_DECRYPT_FAILED);
  if (decrypt_err != APIError::OK) {
    return decrypt_err;
  }

  uint16_t msg_size = mbuf.size;
  uint8_t *msg_data = this->rx_buf_.data();
  if (msg_size < 4) {
    this->state_ = State::FAILED;
    HELPER_LOG("Bad data packet: size %d too short", msg_size);
    return APIError::BAD_DATA_PACKET;
  }

  uint16_t type = (((uint16_t) msg_data[0]) << 8) | msg_data[1];
  uint16_t data_len = (((uint16_t) msg_data[2]) << 8) | msg_data[3];
  if (data_len > msg_size - 4) {
    this->state_ = State::FAILED;
    HELPER_LOG("Bad data packet: data_len %u greater than msg_size %u", data_len, msg_size);
    return APIError::BAD_DATA_PACKET;
  }

  buffer->data = msg_data + 4;  // Skip 4-byte header (type + length)
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

  uint8_t *buffer_data = buffer.get_buffer()->data();

  this->reusable_iovs_.clear();
  this->reusable_iovs_.reserve(packets.size());
  uint16_t total_write_len = 0;

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
    APIError aerr =
        handle_noise_error_(err, LOG_STR("noise_cipherstate_encrypt"), APIError::CIPHERSTATE_ENCRYPT_FAILED);
    if (aerr != APIError::OK)
      return aerr;

    // Fill in the encrypted size
    buf_start[1] = static_cast<uint8_t>(mbuf.size >> 8);
    buf_start[2] = static_cast<uint8_t>(mbuf.size);

    // Add iovec for this encrypted packet
    size_t packet_len = static_cast<size_t>(3 + mbuf.size);  // indicator + size + encrypted data
    this->reusable_iovs_.push_back({buf_start, packet_len});
    total_write_len += packet_len;
  }

  // Send all encrypted packets in one writev call
  return this->write_raw_(this->reusable_iovs_.data(), this->reusable_iovs_.size(), total_write_len);
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
    return this->write_raw_(iov, 1, 3);  // Just header
  }
  iov[1].iov_base = const_cast<uint8_t *>(data);
  iov[1].iov_len = len;

  return this->write_raw_(iov, 2, 3 + len);  // Header + data
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
  APIError aerr =
      handle_noise_error_(err, LOG_STR("noise_handshakestate_new_by_id"), APIError::HANDSHAKESTATE_SETUP_FAILED);
  if (aerr != APIError::OK)
    return aerr;

  const auto &psk = this->ctx_.get_psk();
  err = noise_handshakestate_set_pre_shared_key(handshake_, psk.data(), psk.size());
  aerr = handle_noise_error_(err, LOG_STR("noise_handshakestate_set_pre_shared_key"),
                             APIError::HANDSHAKESTATE_SETUP_FAILED);
  if (aerr != APIError::OK)
    return aerr;

  err = noise_handshakestate_set_prologue(handshake_, prologue_.data(), prologue_.size());
  aerr = handle_noise_error_(err, LOG_STR("noise_handshakestate_set_prologue"), APIError::HANDSHAKESTATE_SETUP_FAILED);
  if (aerr != APIError::OK)
    return aerr;
  // set_prologue copies it into handshakestate, so we can get rid of it now
  prologue_ = {};

  err = noise_handshakestate_start(handshake_);
  aerr = handle_noise_error_(err, LOG_STR("noise_handshakestate_start"), APIError::HANDSHAKESTATE_SETUP_FAILED);
  if (aerr != APIError::OK)
    return aerr;
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
  APIError aerr =
      handle_noise_error_(err, LOG_STR("noise_handshakestate_split"), APIError::HANDSHAKESTATE_SPLIT_FAILED);
  if (aerr != APIError::OK)
    return aerr;

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

}  // namespace esphome::api
#endif  // USE_API_NOISE
#endif  // USE_API
