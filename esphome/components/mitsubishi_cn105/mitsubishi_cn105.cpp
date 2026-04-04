#include <array>
#include <numeric>
#include "mitsubishi_cn105.h"

namespace esphome::mitsubishi_cn105 {

static const char *const TAG = "mitsubishi_cn105.driver";

static constexpr uint32_t WRITE_TIMEOUT_MS = 2000;

static constexpr size_t HEADER_LEN = 5;
static constexpr uint8_t PREAMBLE = 0xFC;
static constexpr uint8_t HEADER_BYTE_1 = 0x01;
static constexpr uint8_t HEADER_BYTE_2 = 0x30;

static constexpr uint8_t PACKET_TYPE_CONNECT_REQUEST = 0x5A;
static constexpr uint8_t PACKET_TYPE_CONNECT_RESPONSE = 0x7A;
static constexpr std::array<uint8_t, 2> CONNECT_REQUEST_PAYLOAD = {{0xCA, 0x01}};

static constexpr uint8_t checksum(const uint8_t *bytes, size_t length) {
  return static_cast<uint8_t>(0xFC - std::accumulate(bytes, bytes + length, uint8_t{0}));
}

template<std::size_t PayloadSize>
static constexpr auto make_packet(uint8_t type, const std::array<uint8_t, PayloadSize> &payload) {
  const size_t full_len = PayloadSize + HEADER_LEN + 1;
  std::array<uint8_t, full_len> packet{PREAMBLE, type, HEADER_BYTE_1, HEADER_BYTE_2, static_cast<uint8_t>(PayloadSize)};
  std::copy_n(payload.begin(), PayloadSize, packet.begin() + HEADER_LEN);
  packet.back() = checksum(packet.data(), packet.size() - 1);
  return packet;
}

static constexpr auto CONNECT_PACKET = make_packet(PACKET_TYPE_CONNECT_REQUEST, CONNECT_REQUEST_PAYLOAD);

void MitsubishiCN105::initialize() { this->set_state_(State::CONNECTING); }

void MitsubishiCN105::update() {
  if (const auto start = this->write_timeout_start_ms_; start && (get_loop_time_ms() - *start) >= WRITE_TIMEOUT_MS) {
    this->write_timeout_start_ms_.reset();
    this->read_pos_ = 0;
    this->set_state_(State::READ_TIMEOUT);
    return;
  }

  this->read_incoming_bytes_();
}

void MitsubishiCN105::set_state_(State new_state) {
  if (should_transition(this->state_, new_state)) {
    ESP_LOGV(TAG, "Did transition: %s -> %s", LOG_STR_ARG(state_to_string(this->state_)),
             LOG_STR_ARG(state_to_string(new_state)));
    this->state_ = new_state;
    this->did_transition_(new_state);
  } else {
    ESP_LOGV(TAG, "Ignoring unexpected transition %s -> %s", LOG_STR_ARG(state_to_string(this->state_)),
             LOG_STR_ARG(state_to_string(new_state)));
  }
}

bool MitsubishiCN105::should_transition(State from, State to) {
  switch (to) {
    case State::CONNECTING:
      return from == State::NOT_CONNECTED || from == State::READ_TIMEOUT;

    case State::CONNECTED:
    case State::READ_TIMEOUT:
      return from == State::CONNECTING;

    default:
      return false;
  }
}

void MitsubishiCN105::did_transition_(State to) {
  switch (to) {
    case State::CONNECTING:
      this->send_packet_(CONNECT_PACKET);
      break;

    case State::CONNECTED:
      this->write_timeout_start_ms_.reset();
      // TODO: read AC status after connected, next PR
      break;

    case State::READ_TIMEOUT:
      this->set_state_(State::CONNECTING);
      break;

    default:
      break;
  }
}

void MitsubishiCN105::send_packet_(const uint8_t *packet, size_t len) {
  dump_buffer_vv("TX", packet, len);
  this->device_.write_array(packet, len);
  this->write_timeout_start_ms_ = get_loop_time_ms();
}

void MitsubishiCN105::read_incoming_bytes_() {
  uint8_t watchdog = 64;
  while (this->device_.available() > 0 && watchdog-- > 0) {
    uint8_t &value = this->read_buffer_[this->read_pos_];
    if (!this->device_.read_byte(&value)) {
      ESP_LOGW(TAG, "UART read failed while data available");
      return;
    }

    switch (++this->read_pos_) {
      case 1:
        if (value != PREAMBLE) {
          this->reset_read_position_and_dump_buffer_("RX ignoring preamble");
        }
        continue;

      case 2:
        continue;

      case 3:
        if (value != HEADER_BYTE_1) {
          this->reset_read_position_and_dump_buffer_("RX invalid: header 1 mismatch");
        }
        continue;

      case 4:
        if (value != HEADER_BYTE_2) {
          this->reset_read_position_and_dump_buffer_("RX invalid: header 2 mismatch");
        }
        continue;

      case HEADER_LEN:
        static_assert(READ_BUFFER_SIZE > HEADER_LEN);
        if (this->read_buffer_[HEADER_LEN - 1] >= READ_BUFFER_SIZE - HEADER_LEN) {
          this->reset_read_position_and_dump_buffer_("RX invalid: payload too large");
        }
        continue;

      default:
        break;
    }

    const size_t len_without_checksum = HEADER_LEN + static_cast<size_t>(this->read_buffer_[HEADER_LEN - 1]);
    if (this->read_pos_ <= len_without_checksum) {
      continue;
    }

    if (checksum(this->read_buffer_, len_without_checksum) != value) {
      this->reset_read_position_and_dump_buffer_("RX invalid: checksum mismatch");
      continue;
    }

    this->process_rx_packet_(this->read_buffer_[1], this->read_buffer_ + HEADER_LEN, len_without_checksum - HEADER_LEN);
    this->reset_read_position_and_dump_buffer_("RX");
  }
}

void MitsubishiCN105::process_rx_packet_(uint8_t type, const uint8_t *payload, size_t len) {
  switch (type) {
    case PACKET_TYPE_CONNECT_RESPONSE:
      this->set_state_(State::CONNECTED);
      break;

    default:
      ESP_LOGVV(TAG, "RX unknown packet type 0x%02X", type);
      break;
  }
}

void MitsubishiCN105::reset_read_position_and_dump_buffer_(const char *prefix) {
  dump_buffer_vv(prefix, this->read_buffer_, this->read_pos_);
  this->read_pos_ = 0;
}

void MitsubishiCN105::dump_buffer_vv(const char *prefix, const uint8_t *data, size_t len) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  char buf[format_hex_pretty_size(READ_BUFFER_SIZE)];
  ESP_LOGVV(TAG, "%s (%zu): %s", prefix, len, format_hex_pretty_to(buf, data, len));
#endif
}

const LogString *MitsubishiCN105::state_to_string(State state) {
  switch (state) {
    case State::NOT_CONNECTED:
      return LOG_STR("Not connected");
    case State::CONNECTING:
      return LOG_STR("Connecting");
    case State::CONNECTED:
      return LOG_STR("Connected");
    case State::READ_TIMEOUT:
      return LOG_STR("ReadTimeout");
  }
  return LOG_STR("Unknown");
}

}  // namespace esphome::mitsubishi_cn105
