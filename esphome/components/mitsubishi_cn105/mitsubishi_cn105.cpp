#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include "mitsubishi_cn105.h"

namespace esphome::mitsubishi_cn105 {

static const char *const TAG = "mitsubishi_cn105.driver";

static constexpr uint32_t WRITE_TIMEOUT_MS = 2000;

static constexpr uint8_t TARGET_TEMPERATURE_ENC_A_OFFSET = 31;

static constexpr size_t REQUEST_PAYLOAD_LEN = 0x10;
static constexpr size_t HEADER_LEN = 5;
static constexpr uint8_t PREAMBLE = 0xFC;
static constexpr uint8_t HEADER_BYTE_1 = 0x01;
static constexpr uint8_t HEADER_BYTE_2 = 0x30;

static constexpr uint8_t PACKET_TYPE_CONNECT_REQUEST = 0x5A;
static constexpr uint8_t PACKET_TYPE_CONNECT_RESPONSE = 0x7A;
static constexpr std::array<uint8_t, 2> CONNECT_REQUEST_PAYLOAD = {0xCA, 0x01};

static constexpr uint8_t PACKET_TYPE_STATUS_REQUEST = 0x42;
static constexpr uint8_t PACKET_TYPE_STATUS_RESPONSE = 0x62;
static constexpr uint8_t STATUS_MSG_SETTINGS = 0x02;
static constexpr uint8_t STATUS_MSG_ROOM_TEMP = 0x03;

static constexpr uint8_t PACKET_TYPE_WRITE_SETTINGS_REQUEST = 0x41;
static constexpr uint8_t PACKET_TYPE_WRITE_SETTINGS_RESPONSE = 0x61;

static constexpr std::array<std::optional<MitsubishiCN105::Mode>, 9> PROTOCOL_MODE_MAP = {
    std::nullopt,                     // 0x00
    MitsubishiCN105::Mode::HEAT,      // 0x01
    MitsubishiCN105::Mode::DRY,       // 0x02
    MitsubishiCN105::Mode::COOL,      // 0x03
    std::nullopt,                     // 0x04
    std::nullopt,                     // 0x05
    std::nullopt,                     // 0x06
    MitsubishiCN105::Mode::FAN_ONLY,  // 0x07
    MitsubishiCN105::Mode::AUTO       // 0x08
};

static constexpr std::array<std::optional<MitsubishiCN105::FanMode>, 7> PROTOCOL_FAN_MODE_MAP = {
    MitsubishiCN105::FanMode::AUTO,     // 0x00
    MitsubishiCN105::FanMode::QUIET,    // 0x01
    MitsubishiCN105::FanMode::SPEED_1,  // 0x02
    MitsubishiCN105::FanMode::SPEED_2,  // 0x03
    std::nullopt,                       // 0x04
    MitsubishiCN105::FanMode::SPEED_3,  // 0x05
    MitsubishiCN105::FanMode::SPEED_4   // 0x06
};

template<typename T, size_t N>
static constexpr std::optional<T> lookup(const std::array<std::optional<T>, N> &table, uint8_t value) {
  return (value < N) ? table[value] : std::nullopt;
}

template<typename T, size_t N>
static constexpr bool reverse_lookup(const std::array<std::optional<T>, N> &table, T value, uint8_t &placeholder) {
  for (size_t i = 0; i < N; ++i) {
    const auto &table_value = table[i];
    if (table_value.has_value() && table_value == value) {
      placeholder = i;
      return true;
    }
  }
  return false;
}

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

static float decode_temperature(int temp_a, int temp_b, int delta) {
  return temp_b != 0 ? (temp_b - 128) / 2.0f : delta + temp_a;
}

static constexpr auto CONNECT_PACKET = make_packet(PACKET_TYPE_CONNECT_REQUEST, CONNECT_REQUEST_PAYLOAD);

void MitsubishiCN105::initialize() { this->set_state_(State::CONNECTING); }

bool MitsubishiCN105::update() {
  if (const auto start = this->status_update_start_ms_) {
    if (this->pending_updates_.any()) {
      this->status_update_wait_credit_ms_ = std::min(this->update_interval_ms_, get_loop_time_ms() - *start);
      this->cancel_waiting_and_transition_to_(State::APPLYING_SETTINGS);
      return false;
    }

    if ((get_loop_time_ms() - *start) >= this->update_interval_ms_) {
      this->cancel_waiting_and_transition_to_(State::UPDATING_STATUS);
      return false;
    }
  }

  if (const auto start = this->write_timeout_start_ms_; start && (get_loop_time_ms() - *start) >= WRITE_TIMEOUT_MS) {
    this->write_timeout_start_ms_.reset();
    this->frame_parser_.reset();
    this->status_update_wait_credit_ms_ = 0;
    this->set_state_(State::READ_TIMEOUT);
    return false;
  }

  return this->frame_parser_.read_and_parse(this->device_, [this](uint8_t type, const uint8_t *payload, size_t len) {
    return this->process_rx_packet_(type, payload, len);
  });
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
      return from == State::CONNECTING;

    case State::UPDATING_STATUS:
      return from == State::CONNECTED || from == State::STATUS_UPDATED ||
             from == State::WAITING_FOR_SCHEDULED_STATUS_UPDATE;

    case State::STATUS_UPDATED:
      return from == State::UPDATING_STATUS;

    case State::SCHEDULE_NEXT_STATUS_UPDATE:
      return from == State::STATUS_UPDATED || from == State::SETTINGS_APPLIED;

    case State::WAITING_FOR_SCHEDULED_STATUS_UPDATE:
      return from == State::SCHEDULE_NEXT_STATUS_UPDATE;

    case State::APPLYING_SETTINGS:
      return from == State::WAITING_FOR_SCHEDULED_STATUS_UPDATE || from == State::STATUS_UPDATED;

    case State::SETTINGS_APPLIED:
      return from == State::APPLYING_SETTINGS;

    case State::READ_TIMEOUT:
      return from == State::UPDATING_STATUS || from == State::APPLYING_SETTINGS || from == State::CONNECTING;

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
      this->current_status_msg_type_ = STATUS_MSG_SETTINGS;
      this->set_state_(State::UPDATING_STATUS);
      break;

    case State::UPDATING_STATUS:
      this->update_status_();
      break;

    case State::STATUS_UPDATED: {
      this->write_timeout_start_ms_.reset();
      if (this->pending_updates_.any() && this->is_status_initialized()) {
        this->set_state_(State::APPLYING_SETTINGS);
      } else if (this->current_status_msg_type_ == STATUS_MSG_SETTINGS && this->should_request_room_temperature_()) {
        this->current_status_msg_type_ = STATUS_MSG_ROOM_TEMP;
        this->set_state_(State::UPDATING_STATUS);
      } else {
        this->set_state_(State::SCHEDULE_NEXT_STATUS_UPDATE);
      }
      break;
    }

    case State::SCHEDULE_NEXT_STATUS_UPDATE:
      this->status_update_start_ms_ = get_loop_time_ms() - this->status_update_wait_credit_ms_;
      this->status_update_wait_credit_ms_ = 0;
      this->current_status_msg_type_ = STATUS_MSG_SETTINGS;
      this->set_state_(State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
      break;

    case State::APPLYING_SETTINGS:
      this->apply_settings_();
      break;

    case State::SETTINGS_APPLIED:
      this->write_timeout_start_ms_.reset();
      this->set_state_(State::SCHEDULE_NEXT_STATUS_UPDATE);
      break;

    case State::READ_TIMEOUT:
      this->set_state_(State::CONNECTING);
      break;

    default:
      break;
  }
}

bool MitsubishiCN105::should_request_room_temperature_() const {
  if (!this->is_room_temperature_enabled()) {
    return false;
  }

  if (!this->last_room_temperature_update_ms_.has_value()) {
    return true;
  }

  return (get_loop_time_ms() - *this->last_room_temperature_update_ms_) >= this->room_temperature_min_interval_ms_;
}

void MitsubishiCN105::send_packet_(const uint8_t *packet, size_t len) {
  FrameParser::dump_buffer_vv("TX", packet, len);
  this->device_.write_array(packet, len);
  this->write_timeout_start_ms_ = get_loop_time_ms();
}

void MitsubishiCN105::update_status_() {
  std::array<uint8_t, REQUEST_PAYLOAD_LEN> payload = {this->current_status_msg_type_};
  this->send_packet_(make_packet(PACKET_TYPE_STATUS_REQUEST, payload));
}

void MitsubishiCN105::cancel_waiting_and_transition_to_(State state) {
  this->status_update_start_ms_.reset();
  this->set_state_(state);
}

bool MitsubishiCN105::process_rx_packet_(uint8_t type, const uint8_t *payload, size_t len) {
  switch (type) {
    case PACKET_TYPE_CONNECT_RESPONSE:
      this->set_state_(State::CONNECTED);
      return false;

    case PACKET_TYPE_STATUS_RESPONSE:
      return this->process_status_packet_(payload, len);

    case PACKET_TYPE_WRITE_SETTINGS_RESPONSE:
      this->set_state_(State::SETTINGS_APPLIED);
      return false;

    default:
      ESP_LOGVV(TAG, "RX unknown packet type 0x%02X", type);
      return false;
  }
}

bool MitsubishiCN105::process_status_packet_(const uint8_t *payload, size_t len) {
  if (len == 0) {
    ESP_LOGVV(TAG, "RX status packet too short");
    return false;
  }

  const auto previous = this->status_;
  const auto msg_type = payload[0];
  if (!this->parse_status_payload_(msg_type, payload + 1, len - 1)) {
    return false;
  }

  if (msg_type == this->current_status_msg_type_) {
    this->set_state_(State::STATUS_UPDATED);
  }

  bool changed = previous.power_on != this->status_.power_on || previous.mode != this->status_.mode ||
                 previous.fan_mode != this->status_.fan_mode ||
                 previous.target_temperature != this->status_.target_temperature;

  if (this->is_room_temperature_enabled()) {
    changed |= previous.room_temperature != this->status_.room_temperature;
  }

  return changed && this->is_status_initialized();
}

bool MitsubishiCN105::parse_status_payload_(uint8_t msg_type, const uint8_t *payload, size_t len) {
  switch (msg_type) {
    case STATUS_MSG_SETTINGS:
      return this->parse_status_settings_(payload, len);

    case STATUS_MSG_ROOM_TEMP:
      return this->parse_status_room_temperature_(payload, len);

    default:
      ESP_LOGVV(TAG, "RX unsupported status msg type 0x%02X", msg_type);
      return false;
  }
}

bool MitsubishiCN105::parse_status_settings_(const uint8_t *payload, size_t len) {
  if (len <= 10) {
    ESP_LOGVV(TAG, "RX settings payload too short");
    return false;
  }

  if (!this->pending_updates_.contains(UpdateFlag::POWER)) {
    this->status_.power_on = payload[2] != 0;
  }

  this->use_temperature_encoding_b_ = payload[10] != 0;
  if (!this->pending_updates_.contains(UpdateFlag::TEMPERATURE)) {
    this->status_.target_temperature = decode_temperature(-payload[4], payload[10], TARGET_TEMPERATURE_ENC_A_OFFSET);
  }

  if (!this->pending_updates_.contains(UpdateFlag::MODE)) {
    const bool i_see = payload[3] > 0x08;
    this->status_.mode = lookup(PROTOCOL_MODE_MAP, payload[3] - (i_see ? 0x08 : 0)).value_or(Mode::UNKNOWN);
  }

  if (!this->pending_updates_.contains(UpdateFlag::FAN)) {
    this->status_.fan_mode = lookup(PROTOCOL_FAN_MODE_MAP, payload[5]).value_or(FanMode::UNKNOWN);
  }

  return true;
}

bool MitsubishiCN105::parse_status_room_temperature_(const uint8_t *payload, size_t len) {
  if (len <= 5) {
    ESP_LOGVV(TAG, "RX room temperature payload too short");
    return false;
  }

  this->status_.room_temperature = decode_temperature(payload[2], payload[5], 10);
  this->last_room_temperature_update_ms_ = get_loop_time_ms();

  return true;
}

void MitsubishiCN105::set_remote_temperature(float temperature) {
  if (std::isnan(temperature)) {
    ESP_LOGD(TAG, "Ignoring NaN remote temperature");
    return;
  }
  if (temperature < 8.0f || temperature > 39.5f) {
    ESP_LOGD(TAG, "Ignoring out-of-range remote temperature: %.1f", temperature);
    return;
  }
  this->set_remote_temperature_half_deg_(static_cast<uint8_t>(std::round(temperature * 2.0f)));
}

void MitsubishiCN105::clear_remote_temperature() {
  this->set_remote_temperature_half_deg_(REMOTE_TEMPERATURE_DISABLED);
}

void MitsubishiCN105::set_remote_temperature_half_deg_(uint8_t temperature_half_deg) {
  this->remote_temperature_half_deg_ = temperature_half_deg;
  this->pending_updates_.set(UpdateFlag::REMOTE_TEMPERATURE);
}

void MitsubishiCN105::set_power(bool power_on) {
  this->status_.power_on = power_on;
  this->pending_updates_.set(UpdateFlag::POWER);
}

void MitsubishiCN105::set_target_temperature(float target_temperature) {
  if (target_temperature < 16 || target_temperature > 31) {
    ESP_LOGD(TAG, "Setting temperature out-of-range: %.1f", target_temperature);
    return;
  }
  this->status_.target_temperature = target_temperature;
  this->pending_updates_.set(UpdateFlag::TEMPERATURE);
}

void MitsubishiCN105::set_mode(Mode mode) {
  uint8_t placeholder;
  if (!reverse_lookup(PROTOCOL_MODE_MAP, mode, placeholder)) {
    ESP_LOGD(TAG, "Setting invalid mode: %u", static_cast<uint8_t>(mode));
    return;
  }
  this->status_.mode = mode;
  this->pending_updates_.set(UpdateFlag::MODE);
}

void MitsubishiCN105::set_fan_mode(FanMode fan_mode) {
  uint8_t placeholder;
  if (!reverse_lookup(PROTOCOL_FAN_MODE_MAP, fan_mode, placeholder)) {
    ESP_LOGD(TAG, "Setting invalid fan mode: %u", static_cast<uint8_t>(fan_mode));
    return;
  }
  this->status_.fan_mode = fan_mode;
  this->pending_updates_.set(UpdateFlag::FAN);
}

void MitsubishiCN105::apply_settings_() {
  std::array<uint8_t, REQUEST_PAYLOAD_LEN> payload{};

  // Apply all other pending settings first; handle REMOTE_TEMPERATURE last
  if (this->pending_updates_.contains_only(UpdateFlag::REMOTE_TEMPERATURE)) {
    payload[0] = 0x07;
    if (this->remote_temperature_half_deg_ == REMOTE_TEMPERATURE_DISABLED) {
      payload[3] = 0x80;
    } else {
      payload[1] = 0x01;
      payload[2] = static_cast<uint8_t>(this->remote_temperature_half_deg_ - 16);
      payload[3] = static_cast<uint8_t>(this->remote_temperature_half_deg_ + 128);
    }
    this->pending_updates_.clear(UpdateFlag::REMOTE_TEMPERATURE);
  } else {
    payload[0] = 0x01;
    if (this->pending_updates_.contains(UpdateFlag::POWER)) {
      payload[1] |= 0x01;
      payload[3] = this->status_.power_on ? 0x01 : 0x00;
    }

    if (this->pending_updates_.contains(UpdateFlag::TEMPERATURE)) {
      payload[1] |= 0x04;
      if (this->use_temperature_encoding_b_) {
        payload[14] = static_cast<uint8_t>(std::round(this->status_.target_temperature * 2.0f) + 128);
      } else {
        payload[5] =
            static_cast<uint8_t>(TARGET_TEMPERATURE_ENC_A_OFFSET - std::round(this->status_.target_temperature));
      }
    }

    if (this->pending_updates_.contains(UpdateFlag::MODE) &&
        reverse_lookup(PROTOCOL_MODE_MAP, this->status_.mode, payload[4])) {
      payload[1] |= 0x02;
    }

    if (this->pending_updates_.contains(UpdateFlag::FAN) &&
        reverse_lookup(PROTOCOL_FAN_MODE_MAP, this->status_.fan_mode, payload[6])) {
      payload[1] |= 0x08;
    }

    this->pending_updates_.clear(UpdateFlag::POWER, UpdateFlag::TEMPERATURE, UpdateFlag::MODE, UpdateFlag::FAN);
  }

  this->send_packet_(make_packet(PACKET_TYPE_WRITE_SETTINGS_REQUEST, payload));
}

const LogString *MitsubishiCN105::state_to_string(State state) {
  switch (state) {
    case State::NOT_CONNECTED:
      return LOG_STR("Not connected");
    case State::CONNECTING:
      return LOG_STR("Connecting");
    case State::CONNECTED:
      return LOG_STR("Connected");
    case State::UPDATING_STATUS:
      return LOG_STR("UpdatingStatus");
    case State::STATUS_UPDATED:
      return LOG_STR("StatusUpdated");
    case State::SCHEDULE_NEXT_STATUS_UPDATE:
      return LOG_STR("ScheduleNextStatusUpdate");
    case State::WAITING_FOR_SCHEDULED_STATUS_UPDATE:
      return LOG_STR("WaitingForScheduledStatusUpdate");
    case State::APPLYING_SETTINGS:
      return LOG_STR("ApplyingSettings");
    case State::SETTINGS_APPLIED:
      return LOG_STR("SettingsApplied");
    case State::READ_TIMEOUT:
      return LOG_STR("ReadTimeout");
  }
  return LOG_STR("Unknown");
}

template<typename Callback>
bool MitsubishiCN105::FrameParser::read_and_parse(uart::UARTDevice &device, Callback &&callback) {
  uint8_t watchdog = 64;
  while (device.available() > 0 && watchdog-- > 0) {
    uint8_t &value = this->read_buffer_[this->read_pos_];
    if (!device.read_byte(&value)) {
      ESP_LOGW(TAG, "UART read failed while data available");
      return false;
    }

    switch (++this->read_pos_) {
      case 1:
        if (value != PREAMBLE) {
          this->reset_and_dump_buffer_("RX ignoring preamble");
        }
        continue;

      case 2:
        continue;

      case 3:
        if (value != HEADER_BYTE_1) {
          this->reset_and_dump_buffer_("RX invalid: header 1 mismatch");
        }
        continue;

      case 4:
        if (value != HEADER_BYTE_2) {
          this->reset_and_dump_buffer_("RX invalid: header 2 mismatch");
        }
        continue;

      case HEADER_LEN:
        static_assert(READ_BUFFER_SIZE > HEADER_LEN);
        if (this->read_buffer_[HEADER_LEN - 1] >= READ_BUFFER_SIZE - HEADER_LEN) {
          this->reset_and_dump_buffer_("RX invalid: payload too large");
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
      this->reset_and_dump_buffer_("RX invalid: checksum mismatch");
      continue;
    }

    dump_buffer_vv("RX", this->read_buffer_, this->read_pos_);
    const bool processed =
        callback(this->read_buffer_[1], this->read_buffer_ + HEADER_LEN, len_without_checksum - HEADER_LEN);
    this->read_pos_ = 0;
    return processed;
  }

  return false;
}

void MitsubishiCN105::FrameParser::reset_and_dump_buffer_(const char *prefix) {
  dump_buffer_vv(prefix, this->read_buffer_, this->read_pos_);
  this->read_pos_ = 0;
}

void MitsubishiCN105::FrameParser::dump_buffer_vv(const char *prefix, const uint8_t *data, size_t len) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  char buf[format_hex_pretty_size(READ_BUFFER_SIZE)];
  ESP_LOGVV(TAG, "%s (%zu): %s", prefix, len, format_hex_pretty_to(buf, data, len));
#endif
}

}  // namespace esphome::mitsubishi_cn105
