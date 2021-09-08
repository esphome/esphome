#include "api_frame_helper.h"

#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "proto.h"

namespace esphome {
namespace api {

static const char *const TAG = "api.socket";

/// Is the given return value (from read/write syscalls) a wouldblock error?
bool is_would_block(ssize_t ret) {
  if (ret == -1) {
    return errno == EWOULDBLOCK || errno == EAGAIN;
  }
  return ret == 0;
}

#define HELPER_LOG(msg, ...) ESP_LOGVV(TAG, "%s: " msg, info_.c_str(), ##__VA_ARGS__)

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
  int err;
  APIError aerr;

  if (frame == nullptr) {
    HELPER_LOG("Bad argument for try_read_frame_");
    return APIError::BAD_ARG;
  }

  // read header
  while (!rx_header_parsed_) {
    uint8_t data;
    ssize_t received = socket_->read(&data, 1);
    if (is_would_block(received)) {
      return APIError::WOULD_BLOCK;
    } else if (received == -1) {
      state_ = State::FAILED;
      HELPER_LOG("Socket read failed with errno %d", errno);
      return APIError::SOCKET_READ_FAILED;
    }
    rx_header_buf_.push_back(data);

    // try parse header
    if (rx_header_buf_[0] != 0x00) {
      state_ = State::FAILED;
      HELPER_LOG("Bad indicator byte %u", rx_header_buf_[0]);
      return APIError::BAD_INDICATOR;
    }

    size_t i = 1;
    size_t consumed = 0;
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
    if (is_would_block(received)) {
      return APIError::WOULD_BLOCK;
    } else if (received == -1) {
      state_ = State::FAILED;
      HELPER_LOG("Socket read failed with errno %d", errno);
      return APIError::SOCKET_READ_FAILED;
    }
    rx_buf_len_ += received;
    if (received != to_read) {
      // not all read
      return APIError::WOULD_BLOCK;
    }
  }

  // uncomment for even more debugging
  // ESP_LOGVV(TAG, "Received frame: %s", hexencode(rx_buf_).c_str());
  frame->msg = std::move(rx_buf_);
  // consume msg
  rx_buf_ = {};
  rx_buf_len_ = 0;
  rx_header_buf_.clear();
  rx_header_parsed_ = false;
  return APIError::OK;
}

APIError APIPlaintextFrameHelper::read_packet(ReadPacketBuffer *buffer) {
  int err;
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
  int err;
  APIError aerr;

  if (state_ != State::DATA) {
    return APIError::BAD_STATE;
  }

  std::vector<uint8_t> header;
  header.push_back(0x00);
  ProtoVarInt(payload_len).encode(header);
  ProtoVarInt(type).encode(header);

  aerr = write_raw_(&header[0], header.size());
  if (aerr != APIError::OK) {
    return aerr;
  }
  aerr = write_raw_(payload, payload_len);
  if (aerr != APIError::OK) {
    return aerr;
  }
  return APIError::OK;
}
APIError APIPlaintextFrameHelper::try_send_tx_buf_() {
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
APIError APIPlaintextFrameHelper::write_raw_(const uint8_t *data, size_t len) {
  if (len == 0)
    return APIError::OK;
  int err;
  APIError aerr;

  // uncomment for even more debugging
  // ESP_LOGVV(TAG, "Sending raw: %s", hexencode(data, len).c_str());

  if (!tx_buf_.empty()) {
    // try to empty tx_buf_ first
    aerr = try_send_tx_buf_();
    if (aerr != APIError::OK && aerr != APIError::WOULD_BLOCK)
      return aerr;
  }

  if (!tx_buf_.empty()) {
    // tx buf not empty, can't write now because then stream would be inconsistent
    tx_buf_.insert(tx_buf_.end(), data, data + len);
    return APIError::OK;
  }

  ssize_t sent = socket_->write(data, len);
  if (is_would_block(sent)) {
    // operation would block, add buffer to tx_buf
    tx_buf_.insert(tx_buf_.end(), data, data + len);
    return APIError::OK;
  } else if (sent == -1) {
    // an error occured
    state_ = State::FAILED;
    HELPER_LOG("Socket write failed with errno %d", errno);
    return APIError::SOCKET_WRITE_FAILED;
  } else if (sent != len) {
    // partially sent, add end to tx_buf
    tx_buf_.insert(tx_buf_.end(), data + sent, data + len);
    return APIError::OK;
  }
  // fully sent
  return APIError::OK;
}
APIError APIPlaintextFrameHelper::write_frame_(const uint8_t *data, size_t len) {
  APIError aerr;

  uint8_t header[3];
  header[0] = 0x01;  // indicator
  header[1] = (uint8_t)(len >> 8);
  header[2] = (uint8_t) len;

  aerr = write_raw_(header, 3);
  if (aerr != APIError::OK)
    return aerr;
  aerr = write_raw_(data, len);
  return aerr;
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

}  // namespace api
}  // namespace esphome
