#include "mitsubishi_cn105.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include "esphome/core/application.h"

namespace esphome {
namespace mitsubishi_cn105 {

static const char *const TAG = "mitsubishi_cn105.driver";

namespace {

constexpr size_t PACKET_SIZE = 22;
constexpr size_t HEADER_LEN = 5;
constexpr uint32_t WRITE_TIMEOUT_MS = 2000;

constexpr uint8_t INFO_MODE_SETTINGS = 0x02;   // settings packet
constexpr uint8_t INFO_MODE_ROOM_TEMP = 0x03;  // current room temperature

constexpr std::array<uint8_t, 2> INFO_MODE = {{INFO_MODE_SETTINGS, INFO_MODE_ROOM_TEMP}};

constexpr std::array<std::optional<ClimateMode>, 9> MODE_MAP = {{
    std::nullopt,           // 0x00
    ClimateMode::HEAT,      // 0x01
    ClimateMode::DRY,       // 0x02
    ClimateMode::COOL,      // 0x03
    std::nullopt,           // 0x04
    std::nullopt,           // 0x05
    std::nullopt,           // 0x06
    ClimateMode::FAN_ONLY,  // 0x07
    ClimateMode::AUTO       // 0x08
}};

constexpr std::array<std::optional<ClimateFanMode>, 7> FAN_MODE_MAP = {{
    ClimateFanMode::AUTO,     // 0x00
    ClimateFanMode::QUIET,    // 0x01
    ClimateFanMode::SPEED_1,  // 0x02
    ClimateFanMode::SPEED_2,  // 0x03
    std::nullopt,             // 0x04
    ClimateFanMode::SPEED_3,  // 0x05
    ClimateFanMode::SPEED_4   // 0x06
}};

#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE

const char *climate_mode_to_string(ClimateMode mode) {
  switch (mode) {
    case ClimateMode::HEAT:
      return "HEAT";
    case ClimateMode::DRY:
      return "DRY";
    case ClimateMode::COOL:
      return "COOL";
    case ClimateMode::FAN_ONLY:
      return "FAN_ONLY";
    case ClimateMode::AUTO:
      return "AUTO";
    case ClimateMode::UNKNOWN:
      return "UNKNOWN";
  }
  return "INVALID";
}

const char *climate_fan_mode_to_string(ClimateFanMode mode) {
  switch (mode) {
    case ClimateFanMode::AUTO:
      return "AUTO";
    case ClimateFanMode::QUIET:
      return "QUIET";
    case ClimateFanMode::SPEED_1:
      return "SPEED_1";
    case ClimateFanMode::SPEED_2:
      return "SPEED_2";
    case ClimateFanMode::SPEED_3:
      return "SPEED_3";
    case ClimateFanMode::SPEED_4:
      return "SPEED_4";
    case ClimateFanMode::UNKNOWN:
      return "UNKNOWN";
  }
  return "INVALID";
}

#endif

constexpr uint8_t checksum(const uint8_t *bytes, size_t length) {
  uint8_t sum = 0;
  while (length--) {
    sum += *bytes++;
  }
  return static_cast<uint8_t>(0xFC - sum);
}

}  // namespace

void MitsubishiCN105::init() { this->set_state_(State::CONNECTING); }

bool MitsubishiCN105::sync() {
  if (const auto status_update_start_ms = this->status_update_start_ms_) {
    if (pending_updates_.any()) {
      this->cancel_waiting_and_transit_to_(State::APPLYING_SETTINGS);
      return false;
    }

    if ((App.get_loop_component_start_time() - *status_update_start_ms) >= this->update_interval_ms_) {
      this->cancel_waiting_and_transit_to_(State::UPDATING_STATUS);
      return false;
    }
  }

  if (this->write_timeout_start_ms_.has_value() &&
      (App.get_loop_component_start_time() - *this->write_timeout_start_ms_) >= WRITE_TIMEOUT_MS) {
    this->write_timeout_start_ms_.reset();
    this->read_pos_ = 0;
    this->set_state_(State::READ_TIMEOUT);
    return false;
  }

  return this->read_incoming_bytes_();
}

void MitsubishiCN105::set_state_(State new_state) {
  if (new_state != this->state_ && should_transition(this->state_, new_state)) {
    ESP_LOGD(TAG, "Did transition: %s -> %s", state_to_string(this->state_), state_to_string(new_state));
    const auto prev_state = this->state_;
    this->state_ = new_state;
    this->did_transition_(prev_state, new_state);
  } else {
    ESP_LOGW(TAG, "Ignoring transition %s -> %s", state_to_string(this->state_), state_to_string(new_state));
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
      if (pending_updates_.any() && this->status_initialized_) {
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
      this->status_update_start_ms_ = App.get_loop_component_start_time();
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

void MitsubishiCN105::cancel_waiting_and_transit_to_(State state) {
  this->status_update_start_ms_.reset();
  this->set_state_(state);
}

void MitsubishiCN105::apply_settings_() {
  const auto settings = this->current_status_.settings;

  ESP_LOGV(TAG, "Applying settings to AC:");

  if (this->pending_updates_.has(UpdateFlag::POWER)) {
    ESP_LOGV(TAG, "  power=%s", settings.power_on ? "ON" : "OFF");
  }

  if (this->pending_updates_.has(UpdateFlag::MODE)) {
    ESP_LOGV(TAG, "  mode=%s", climate_mode_to_string(settings.mode));
  }

  if (this->pending_updates_.has(UpdateFlag::TEMPERATURE)) {
    ESP_LOGV(TAG, "  target_temp=%.1f", settings.target_temperature);
  }

  if (this->pending_updates_.has(UpdateFlag::FAN)) {
    ESP_LOGV(TAG, "  fan_mode=%s", climate_fan_mode_to_string(settings.fan_mode));
  }

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
  this->transport_.write_array(packet, size);
  this->transport_.flush();
  this->write_timeout_start_ms_ = App.get_loop_component_start_time();
}

void MitsubishiCN105::update_status_() {
  ESP_LOGV(TAG, "Requesting status update, info_mode_index=%u", this->info_mode_index_);
  uint8_t packet[PACKET_SIZE] = {0xFC, 0x42, 0x01, 0x30, 0x10, INFO_MODE[this->info_mode_index_]};
  packet[PACKET_SIZE - 1] = checksum(packet, PACKET_SIZE - 1);
  this->send_packet_(packet);
}

bool MitsubishiCN105::read_incoming_bytes_() {
  while (this->transport_.available() > 0) {
    uint8_t value;
    if (!this->transport_.read_byte(&value)) {
      ESP_LOGW(TAG, "UART read failed while data available");
      return false;
    }

    ESP_LOGVV(TAG, "RX byte: %02X", value);

    // Wait for start byte
    if (this->read_pos_ == 0) {
      if (value == 0xFC) {
        this->add_byte_to_read_buffer_(value);
      } else {
        ESP_LOGW(TAG, "RX ignoring preamble: %02X", value);
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
    if (this->read_pos_ < expected_len_without_checksum) {
      this->add_byte_to_read_buffer_(value);
      continue;
    }

    // Packet complete: current byte (value) is checksum
    const uint8_t received_checksum = value;
    const uint8_t length = static_cast<uint8_t>(this->read_pos_);
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
    ESP_LOGW(TAG, "RX status packet too short (len=%u)", (unsigned) length);
    return false;
  }

  const auto previous = this->current_status_;

  if (!this->parse_values_(packet + HEADER_LEN, length - HEADER_LEN)) {
    return false;
  }

  this->set_state_(State::STATUS_UPDATED);

  if (previous != this->current_status_ && this->status_initialized_) {
    ESP_LOGD(TAG, "Status changed");
    return true;
  }

  return false;
}

std::optional<MitsubishiCN105::State> MitsubishiCN105::check_incoming_packet(const uint8_t *packet, uint8_t length,
                                                                             uint8_t received_checksum) {
  // read_incoming_bytes_ ensures we have at least HEADER_LEN bytes in packet
  static_assert(HEADER_LEN > 3, "HEADER_LEN must be at least 4 to safely access packet[3]");

  if (packet[2] != 0x01 || packet[3] != 0x30) {
    ESP_LOGW(TAG, "RX invalid: header mismatch (b2=%02X b3=%02X)", (unsigned) packet[2], (unsigned) packet[3]);
    return std::nullopt;
  }

  const uint8_t calculated_checksum = checksum(packet, length);
  if (calculated_checksum != received_checksum) {
    ESP_LOGW(TAG, "RX invalid: checksum mismatch (recv=%02X calc=%02X)", (unsigned) received_checksum,
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
      ESP_LOGW(TAG, "RX invalid: unknown packet type 0x%02X (len=%u)", (unsigned) packet[1], (unsigned) length);
      return std::nullopt;
  }
}

bool MitsubishiCN105::parse_values_(const uint8_t *data, size_t length) {
  // process_incoming_packet_ ensures we have at least one byte in data
  switch (*data) {
    case INFO_MODE_SETTINGS: {
      // Accesses up to data[11]
      if (length < 12) {
        ESP_LOGW(TAG, "Settings payload too short: len=%u (expected >=12)", (unsigned) length);
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
      this->apply_to_(UpdateFlag::MODE, MODE_MAP, mode, [this](const std::optional<ClimateMode> &mode) {
        this->current_status_.settings.mode = mode.value_or(ClimateMode::UNKNOWN);
      });

      // Fan
      this->apply_to_(UpdateFlag::FAN, FAN_MODE_MAP, fan, [this](const std::optional<ClimateFanMode> &fan_mode) {
        this->current_status_.settings.fan_mode = fan_mode.value_or(ClimateFanMode::UNKNOWN);
      });

      ESP_LOGV(TAG, "Parsed settings: power=%s(%u) mode=%s(%u) temp=%.1f fan=%s(%u) i-see=%s",
               this->current_status_.settings.power_on ? "ON" : "OFF", power,
               climate_mode_to_string(this->current_status_.settings.mode), mode,
               this->current_status_.settings.target_temperature,
               climate_fan_mode_to_string(this->current_status_.settings.fan_mode), fan, i_see ? "ON" : "OFF");

      return true;
    }

    case INFO_MODE_ROOM_TEMP: {
      // Accesses up to data[6]
      if (length < 7) {
        ESP_LOGW(TAG, "Room temperature payload too short: len=%u (expected >=7)", (unsigned) length);
        return false;
      }

      const uint8_t room_main = data[3];
      const uint8_t room_alt = data[6];
      float room_temp;

      if (room_alt != 0x00) {
        room_temp = (static_cast<float>(room_alt) - 128.0f) / 2.0f;
      } else {
        room_temp = static_cast<float>(room_main) + 10.0f;
      }

      this->current_status_.room_temperature = room_temp;
      ESP_LOGV(TAG, "Parsed room temperature=%.1f", room_temp);

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
    ESP_LOGW(TAG, "RX buffer overflow, resetting");
    dump_buffer_vv("RX partial: ", this->read_buffer_, this->read_pos_);
    this->read_pos_ = 0;
  }
}

void MitsubishiCN105::set_target_temperature(float target_temperature) {
  if (target_temperature < 16 || target_temperature > 31) {
    ESP_LOGW(TAG, "Setting temperature out-of-range: %.1f", target_temperature);
    return;
  }
  target_temperature = std::lroundf(target_temperature);
  ESP_LOGV(TAG, "Setting temperature to: %.1f", target_temperature);
  this->current_status_.settings.target_temperature = target_temperature;
  this->pending_updates_.set(UpdateFlag::TEMPERATURE);
}

void MitsubishiCN105::set_power(bool power_on) {
  ESP_LOGV(TAG, "Setting power to: %u", power_on);
  this->current_status_.settings.power_on = power_on;
  this->pending_updates_.set(UpdateFlag::POWER);
}

void MitsubishiCN105::set_mode(ClimateMode mode) {
  if (mode == ClimateMode::UNKNOWN) {
    ESP_LOGW(TAG, "Setting invalid mode value");
    return;
  }
  ESP_LOGV(TAG, "Setting mode to: %s", climate_mode_to_string(mode));
  this->current_status_.settings.mode = mode;
  this->pending_updates_.set(UpdateFlag::MODE);
}

void MitsubishiCN105::set_fan_mode(ClimateFanMode fan_mode) {
  if (fan_mode == ClimateFanMode::UNKNOWN) {
    ESP_LOGW(TAG, "Setting invalid fan mode value");
    return;
  }
  ESP_LOGV(TAG, "Setting fan mode to: %s", climate_fan_mode_to_string(fan_mode));
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
    ESP_LOGV(TAG, "Ignoring apply due pending update: flag=%s, value=%u", update_flag_to_string(flag), value);
    return;
  }

  if (const auto mapped = (value < N) ? table[value] : std::nullopt) {
    callback(mapped);
  } else {
    callback(std::nullopt);
    ESP_LOGW(TAG, "Lookup failed: flag=%s, value=%u", update_flag_to_string(flag), value);
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

  ESP_LOGW(TAG, "Reverse lookup failed: flag=%s, value=%d", update_flag_to_string(flag), static_cast<int>(value));
  return std::nullopt;
}

void MitsubishiCN105::dump_buffer_vv(const char *prefix, const uint8_t *data, size_t len) {
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERY_VERBOSE
  char buf[format_hex_pretty_size(READ_BUFFER_SIZE)];
#endif
  ESP_LOGVV(TAG, "%s (%u): %s", prefix, (unsigned) len, format_hex_pretty_to(buf, data, len));
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

const char *MitsubishiCN105::update_flag_to_string(UpdateFlag flag) {
  switch (flag) {
    case UpdateFlag::TEMPERATURE:
      return "TEMPERATURE";
    case UpdateFlag::POWER:
      return "POWER";
    case UpdateFlag::MODE:
      return "MODE";
    case UpdateFlag::FAN:
      return "FAN";
    default:
      return "UNKNOWN";
  }
}

}  // namespace mitsubishi_cn105
}  // namespace esphome
