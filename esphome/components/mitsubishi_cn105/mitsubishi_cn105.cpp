#include "mitsubishi_cn105.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome::mitsubishi_cn105 {

static const char *const TAG = "mitsubishi_cn105.driver";

constexpr size_t PACKET_SIZE = 22;
constexpr size_t HEADER_LEN = 5;
constexpr uint32_t WRITE_TIMEOUT_MS = 2000;

constexpr uint8_t INFO_MODE_SETTINGS = 0x02;   // settings packet
constexpr uint8_t INFO_MODE_ROOM_TEMP = 0x03;  // current room temperature

constexpr std::array<uint8_t, 2> INFO_MODE = {{INFO_MODE_SETTINGS, INFO_MODE_ROOM_TEMP}};

static constexpr std::array<std::optional<MitsubishiCN105::Mode>, 9> MODE_MAP = {{
    std::nullopt,                     // 0x00
    MitsubishiCN105::Mode::HEAT,      // 0x01
    MitsubishiCN105::Mode::DRY,       // 0x02
    MitsubishiCN105::Mode::COOL,      // 0x03
    std::nullopt,                     // 0x04
    std::nullopt,                     // 0x05
    std::nullopt,                     // 0x06
    MitsubishiCN105::Mode::FAN_ONLY,  // 0x07
    MitsubishiCN105::Mode::AUTO       // 0x08
}};

static constexpr std::array<std::optional<MitsubishiCN105::FanMode>, 7> FAN_MODE_MAP = {{
    MitsubishiCN105::FanMode::AUTO,     // 0x00
    MitsubishiCN105::FanMode::QUIET,    // 0x01
    MitsubishiCN105::FanMode::SPEED_1,  // 0x02
    MitsubishiCN105::FanMode::SPEED_2,  // 0x03
    std::nullopt,                       // 0x04
    MitsubishiCN105::FanMode::SPEED_3,  // 0x05
    MitsubishiCN105::FanMode::SPEED_4   // 0x06
}};

static constexpr uint8_t checksum(const uint8_t *bytes, size_t length) {
  uint8_t sum = 0;
  while (length--) {
    sum += *bytes++;
  }
  return static_cast<uint8_t>(0xFC - sum);
}

void MitsubishiCN105::init() { this->set_state_(State::CONNECTING); }

bool MitsubishiCN105::sync() {
  if (const auto status_update_start_ms = this->status_update_start_ms_) {
    if (this->pending_updates_.any()) {
      this->cancel_waiting_and_transition_to_(State::APPLYING_SETTINGS);
      return false;
    }

    if ((this->now() - *status_update_start_ms) >= this->update_interval_ms_) {
      this->cancel_waiting_and_transition_to_(State::UPDATING_STATUS);
      return false;
    }
  }

  if (this->write_timeout_start_ms_.has_value() && (this->now() - *this->write_timeout_start_ms_) >= WRITE_TIMEOUT_MS) {
    this->write_timeout_start_ms_.reset();
    this->read_pos_ = 0;
    this->set_state_(State::READ_TIMEOUT);
    return false;
  }

  return this->read_incoming_bytes_();
}

void MitsubishiCN105::set_state_(State new_state) {
  if (new_state != this->state_ && should_transition(this->state_, new_state)) {
    ESP_LOGV(TAG, "Did transition: %s -> %s", state_to_string(this->state_), state_to_string(new_state));
    const auto prev_state = this->state_;
    this->state_ = new_state;
    this->did_transition_(prev_state, new_state);
  } else {
    ESP_LOGD(TAG, "Ignoring transition %s -> %s", state_to_string(this->state_), state_to_string(new_state));
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

void MitsubishiCN105::did_transition_(State from, State to) {
  switch (to) {
    case State::READ_TIMEOUT:
      if (from == State::CONNECTING) {
        this->connection_state_callback_(false);
      }
      this->set_state_(State::CONNECTING);
      break;

    case State::CONNECTING:
      this->connect_();
      break;

    case State::CONNECTED:
      this->connection_state_callback_(true);
      this->response_received_();
      this->info_mode_index_ = 0;
      this->set_state_(State::UPDATING_STATUS);
      break;

    case State::UPDATING_STATUS:
      this->update_status_();
      break;

    case State::STATUS_UPDATED: {
      this->response_received_();
      if (++this->info_mode_index_ >= INFO_MODE.size()) {
        this->info_mode_index_ = 0;
      }
      if (this->pending_updates_.any() && this->status_initialized_) {
        this->set_state_(State::APPLYING_SETTINGS);
      } else if (this->info_mode_index_ != 0) {
        this->set_state_(State::UPDATING_STATUS);
      } else {
        this->status_initialized_ = true;
        this->set_state_(State::SCHEDULE_NEXT_STATUS_UPDATE);
      }
      break;
    }

    case State::SCHEDULE_NEXT_STATUS_UPDATE:
      this->status_update_start_ms_ = this->now();
      this->set_state_(State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
      break;

    case State::APPLYING_SETTINGS:
      this->apply_settings_();
      this->pending_updates_.clear();
      break;

    case State::SETTINGS_APPLIED:
      this->response_received_();
      this->set_state_(State::SCHEDULE_NEXT_STATUS_UPDATE);
      break;

    default:
      break;
  }
}

void MitsubishiCN105::cancel_waiting_and_transition_to_(State state) {
  this->status_update_start_ms_.reset();
  this->set_state_(state);
}

void MitsubishiCN105::apply_settings_() {
  const auto settings = this->current_status_.settings;

  uint8_t packet[PACKET_SIZE] = {0xfc, 0x41, 0x01, 0x30, 0x10, 0x01};

  if (this->pending_updates_.has(UpdateFlag::POWER)) {
    packet[8] = settings.power_on ? 0x01 : 0x00;
    packet[6] |= 0x01;
  }

  if (this->pending_updates_.has(UpdateFlag::TEMPERATURE)) {
    packet[6] |= 0x04;

    if (this->temp_mode_) {
      packet[19] = static_cast<uint8_t>(settings.target_temperature * 2.0f + 128.0f);
    } else {
      packet[10] = 31 - settings.target_temperature;
    }
  }

  if (const auto mode = this->pending_update_for_(UpdateFlag::MODE, MODE_MAP, settings.mode)) {
    packet[9] = *mode;
    packet[6] |= 0x02;
  }

  if (const auto fan_mode = this->pending_update_for_(UpdateFlag::FAN, FAN_MODE_MAP, settings.fan_mode)) {
    packet[11] = *fan_mode;
    packet[6] |= 0x08;
  }

  packet[PACKET_SIZE - 1] = checksum(packet, PACKET_SIZE - 1);
  this->send_packet_(packet);
}

void MitsubishiCN105::response_received_() { this->write_timeout_start_ms_.reset(); }

void MitsubishiCN105::connect_() {
  static constexpr uint8_t CONNECT_PACKET[] = {0xFC, 0x5A, 0x01, 0x30, 0x02, 0xCA, 0x01, 0xA8};
  this->send_packet_(CONNECT_PACKET);
}

void MitsubishiCN105::send_packet_(const uint8_t *packet, size_t size) {
  dump_buffer_vv("TX", packet, size);
  this->device_.write_array(packet, size);
  this->write_timeout_start_ms_ = this->now();
}

void MitsubishiCN105::update_status_() {
  ESP_LOGV(TAG, "Requesting status update, info_mode_index=%u", this->info_mode_index_);
  uint8_t packet[PACKET_SIZE] = {0xFC, 0x42, 0x01, 0x30, 0x10, INFO_MODE[this->info_mode_index_]};
  packet[PACKET_SIZE - 1] = checksum(packet, PACKET_SIZE - 1);
  this->send_packet_(packet);
}

bool MitsubishiCN105::read_incoming_bytes_() {
  while (this->device_.available() > 0) {
    uint8_t value;
    if (!this->device_.read_byte(&value)) {
      ESP_LOGW(TAG, "UART read failed while data available");
      return false;
    }

    ESP_LOGVV(TAG, "RX byte: %02X", value);

    // Wait for start byte
    if (this->read_pos_ == 0) {
      if (value == 0xFC) {
        this->add_byte_to_read_buffer_(value);
      } else {
        ESP_LOGD(TAG, "RX ignoring preamble: %02X", value);
      }
      continue;
    }

    // Read header first (0..4)
    if (this->read_pos_ < HEADER_LEN) {
      this->add_byte_to_read_buffer_(value);
      continue;
    }

    // Compute expected length without checksum
    const uint8_t data_len = this->read_buffer_[HEADER_LEN - 1];
    const size_t expected_len_without_checksum = HEADER_LEN + static_cast<size_t>(data_len);

    // Read payload (without checksum)
    if (static_cast<size_t>(this->read_pos_) < expected_len_without_checksum) {
      this->add_byte_to_read_buffer_(value);
      continue;
    }

    // Packet complete: current byte (value) is checksum
    const uint8_t received_checksum = value;
    const uint8_t length = this->read_pos_;
    this->read_pos_ = 0;

    dump_buffer_vv("RX", this->read_buffer_, length);

    return this->process_incoming_packet_(this->read_buffer_, length, received_checksum);
  }

  return false;
}

bool MitsubishiCN105::process_incoming_packet_(const uint8_t *packet, uint8_t length, uint8_t received_checksum) {
  const auto status = check_incoming_packet(packet, length, received_checksum);
  if (!status.has_value()) {
    return false;
  }

  if (*status != State::STATUS_UPDATED) {
    this->set_state_(*status);
    return false;
  }

  if (length <= HEADER_LEN) {
    ESP_LOGD(TAG, "RX status packet too short (len=%u)", (unsigned) length);
    return false;
  }

  const auto previous = this->current_status_;

  const uint8_t *payload = packet + HEADER_LEN;
  if (!this->parse_values_(payload, length - HEADER_LEN)) {
    return false;
  }

  // Transition to STATUS_UPDATED only if the received status update matches the requested type
  // while parsed values can still be published if changed
  if (*payload == INFO_MODE[this->info_mode_index_]) {
    this->set_state_(State::STATUS_UPDATED);
  }

  if (previous == this->current_status_ || !this->status_initialized_) {
    return false;
  }

  ESP_LOGD(TAG,
           "Status changed: "
           "power=%s->%s mode=%u->%u target=%.1f->%.1f fan=%u->%u room=%.1f->%.1f",
           previous.settings.power_on ? "ON" : "OFF", this->current_status_.settings.power_on ? "ON" : "OFF",
           static_cast<uint8_t>(previous.settings.mode), static_cast<uint8_t>(this->current_status_.settings.mode),
           previous.settings.target_temperature, this->current_status_.settings.target_temperature,
           static_cast<uint8_t>(previous.settings.fan_mode),
           static_cast<uint8_t>(this->current_status_.settings.fan_mode), previous.room_temperature,
           this->current_status_.room_temperature);

  return true;
}

std::optional<MitsubishiCN105::State> MitsubishiCN105::check_incoming_packet(const uint8_t *packet, uint8_t length,
                                                                             uint8_t received_checksum) {
  // read_incoming_bytes_ ensures we have at least HEADER_LEN bytes in packet
  static_assert(HEADER_LEN > 3, "HEADER_LEN must be at least 4 to safely access packet[3]");

  if (packet[2] != 0x01 || packet[3] != 0x30) {
    ESP_LOGD(TAG, "RX invalid: header mismatch (b2=%02X b3=%02X)", (unsigned) packet[2], (unsigned) packet[3]);
    return std::nullopt;
  }

  const uint8_t calculated_checksum = checksum(packet, length);
  if (calculated_checksum != received_checksum) {
    ESP_LOGD(TAG, "RX invalid: checksum mismatch (recv=%02X calc=%02X)", (unsigned) received_checksum,
             (unsigned) calculated_checksum);
    return std::nullopt;
  }

  switch (packet[1]) {
    case 0x61:
      return State::SETTINGS_APPLIED;
    case 0x62:
      return State::STATUS_UPDATED;
    case 0x7A:
      return State::CONNECTED;
    default:
      ESP_LOGD(TAG, "RX invalid: unknown packet type 0x%02X (len=%u)", (unsigned) packet[1], (unsigned) length);
      return std::nullopt;
  }
}

bool MitsubishiCN105::parse_values_(const uint8_t *data, size_t length) {
  // process_incoming_packet_ ensures we have at least one byte in data
  switch (*data) {
    case INFO_MODE_SETTINGS: {
      // Accesses up to data[11]
      if (length < 12) {
        ESP_LOGD(TAG, "Settings payload too short: len=%u (expected >=12)", (unsigned) length);
        return false;
      }

      const uint8_t power = data[3];
      const uint8_t mode_raw = data[4];
      const uint8_t temp_alt = data[11];
      const uint8_t temp_main = data[5];
      const uint8_t fan = data[6];
      const bool i_see = mode_raw > 0x08;
      const uint8_t mode = i_see ? static_cast<uint8_t>(mode_raw - 0x08) : mode_raw;

      // Power
      if (!this->pending_updates_.has(UpdateFlag::POWER)) {
        this->current_status_.settings.power_on = power != 0;
      } else {
        ESP_LOGV(TAG, "Ignoring POWER due pending update");
      }

      // Target temp
      this->temp_mode_ = temp_alt != 0x00;
      if (!this->pending_updates_.has(UpdateFlag::TEMPERATURE)) {
        if (this->temp_mode_) {
          this->current_status_.settings.target_temperature = (static_cast<float>(temp_alt) - 128.0f) / 2.0f;
        } else {
          this->current_status_.settings.target_temperature = 31.0f - static_cast<float>(temp_main);
        }
      } else {
        ESP_LOGV(TAG, "Ignoring TARGET TEMP due pending update");
      }

      // Mode
      this->apply_to_(UpdateFlag::MODE, MODE_MAP, mode, [this](const std::optional<Mode> &mode) {
        this->current_status_.settings.mode = mode.value_or(Mode::UNKNOWN);
      });

      // Fan
      this->apply_to_(UpdateFlag::FAN, FAN_MODE_MAP, fan, [this](const std::optional<FanMode> &fan_mode) {
        this->current_status_.settings.fan_mode = fan_mode.value_or(FanMode::UNKNOWN);
      });

      ESP_LOGV(TAG, "Parsed settings: power=%s(%u) mode=%u temp=%.1f fan=%u i-see=%s",
               this->current_status_.settings.power_on ? "ON" : "OFF", power, mode,
               this->current_status_.settings.target_temperature, fan, i_see ? "ON" : "OFF");

      return true;
    }

    case INFO_MODE_ROOM_TEMP: {
      // Accesses up to data[6]
      if (length < 7) {
        ESP_LOGD(TAG, "Room temperature payload too short: len=%u (expected >=7)", (unsigned) length);
        return false;
      }

      const uint8_t room_main = data[3];
      const uint8_t room_alt = data[6];

      if (room_alt != 0x00) {
        this->current_status_.room_temperature = (static_cast<float>(room_alt) - 128.0f) / 2.0f;
      } else {
        this->current_status_.room_temperature = static_cast<float>(room_main) + 10.0f;
      }

      ESP_LOGV(TAG, "Parsed room temperature=%.1f", this->current_status_.room_temperature);

      return true;
    }

    default:
      ESP_LOGV(TAG, "Ignoring unsupported payload type: 0x%02X (len=%u)", data[0], (unsigned) length);
      return false;
  }
}

void MitsubishiCN105::add_byte_to_read_buffer_(uint8_t value) {
  if (this->read_pos_ < READ_BUFFER_SIZE) {
    this->read_buffer_[this->read_pos_] = value;
    ++this->read_pos_;
  } else {
    ESP_LOGD(TAG, "RX buffer overflow, resetting");
    dump_buffer_vv("RX partial: ", this->read_buffer_, this->read_pos_);
    this->read_pos_ = 0;
  }
}

void MitsubishiCN105::set_target_temperature(float target_temperature) {
  if (target_temperature < 16 || target_temperature > 31) {
    ESP_LOGD(TAG, "Setting temperature out-of-range: %.1f", target_temperature);
    return;
  }
  target_temperature = std::round(target_temperature * 2.0f) * 0.5f;
  this->current_status_.settings.target_temperature = target_temperature;
  this->pending_updates_.set(UpdateFlag::TEMPERATURE);
}

void MitsubishiCN105::set_power(bool power_on) {
  this->current_status_.settings.power_on = power_on;
  this->pending_updates_.set(UpdateFlag::POWER);
}

void MitsubishiCN105::set_mode(Mode mode) {
  if (mode == Mode::UNKNOWN) {
    ESP_LOGD(TAG, "Setting invalid mode value");
    return;
  }
  this->current_status_.settings.mode = mode;
  this->pending_updates_.set(UpdateFlag::MODE);
}

void MitsubishiCN105::set_fan_mode(FanMode fan_mode) {
  if (fan_mode == FanMode::UNKNOWN) {
    ESP_LOGD(TAG, "Setting invalid fan mode value");
    return;
  }
  this->current_status_.settings.fan_mode = fan_mode;
  this->pending_updates_.set(UpdateFlag::FAN);
}

void MitsubishiCN105::set_connection_state_callback(std::function<void(bool)> &&callback) {
  this->connection_state_callback_ = std::move(callback);
}

template<typename T, size_t N, typename F>
void MitsubishiCN105::apply_to_(UpdateFlag flag, const std::array<std::optional<T>, N> &table, uint8_t value,
                                F &&callback) const {
  if (this->pending_updates_.has(flag)) {
    ESP_LOGV(TAG, "Ignoring apply due pending update: flag=%d, value=%u", static_cast<int>(flag), value);
    return;
  }

  if (const auto mapped = (value < N) ? table[value] : std::nullopt) {
    callback(mapped);
  } else {
    callback(std::nullopt);
    ESP_LOGD(TAG, "Lookup failed: flag=%d, value=%u", static_cast<int>(flag), value);
  }
}

template<typename T, size_t N>
std::optional<uint8_t> MitsubishiCN105::pending_update_for_(UpdateFlag flag, const std::array<std::optional<T>, N> &map,
                                                            T value) const {
  if (!this->pending_updates_.has(flag)) {
    return std::nullopt;
  }

  for (size_t i = 0; i < N; ++i) {
    if (map[i] == value) {
      return i;
    }
  }

  ESP_LOGD(TAG, "Reverse lookup failed: flag=%d, value=%d", static_cast<int>(flag), static_cast<int>(value));
  return std::nullopt;
}

void MitsubishiCN105::dump_buffer_vv(const char *prefix, const uint8_t *data, size_t len) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  char buf[format_hex_pretty_size(READ_BUFFER_SIZE)];
  ESP_LOGVV(TAG, "%s (%u): %s", prefix, (unsigned) len, format_hex_pretty_to(buf, data, len));
#endif
}

const char *MitsubishiCN105::state_to_string(State state) {
  switch (state) {
    case State::NOT_CONNECTED:
      return "Not connected";
    case State::CONNECTING:
      return "Connecting";
    case State::CONNECTED:
      return "Connected";
    case State::UPDATING_STATUS:
      return "UpdatingStatus";
    case State::STATUS_UPDATED:
      return "StatusUpdated";
    case State::SCHEDULE_NEXT_STATUS_UPDATE:
      return "ScheduleNextStatusUpdate";
    case State::WAITING_FOR_SCHEDULED_STATUS_UPDATE:
      return "WaitingForScheduledStatusUpdate";
    case State::APPLYING_SETTINGS:
      return "ApplyingSettings";
    case State::SETTINGS_APPLIED:
      return "SettingsApplied";
    case State::READ_TIMEOUT:
      return "ReadTimeout";
  }
  return "Unknown";
}

}  // namespace esphome::mitsubishi_cn105
