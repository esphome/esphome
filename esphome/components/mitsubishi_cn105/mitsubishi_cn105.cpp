#include "mitsubishi_cn105.h"

#include <algorithm>
#include <array>
#include <cmath>
#include "mitsubishi_cn105_properties.h"

namespace esphome::mitsubishi_cn105 {

ESPHOME_LOG_TAG(TAG, "mitsubishi_cn105.driver");

static constexpr uint32_t RESPONSE_TIMEOUT_MS = 2000;

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
static constexpr uint8_t STATUS_MSG_NONE = 0x00;
static constexpr uint8_t STATUS_MSG_SETTINGS = 0x02;
static constexpr uint8_t STATUS_MSG_ROOM_TEMPERATURE = 0x03;
static constexpr uint8_t STATUS_MSG_OPERATING_STATUS = 0x06;

static constexpr uint8_t PACKET_TYPE_WRITE_SETTINGS_REQUEST = 0x41;
static constexpr uint8_t PACKET_TYPE_WRITE_SETTINGS_RESPONSE = 0x61;

// After a power-off or a mode change that ends the run, 0x06 is read this often while the unit winds down.
static constexpr uint32_t WIND_DOWN_READ_INTERVAL_MS = 10000;

static constexpr uint8_t checksum(const uint8_t *bytes, size_t length) {
  return static_cast<uint8_t>(0xFC - sum_bytes(bytes, length));
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

bool MitsubishiCN105::update() {
  switch (this->state_) {
    case State::DEFERRED_STATUS_REQUEST:
      // Defer the next request to a later loop iteration; some units might not respond if a request is sent
      // immediately after a response. See https://github.com/esphome/esphome/issues/18099. No minimum RX-to-TX delay
      // is enforced.
      this->set_state_(State::UPDATING_STATUS);
      return false;

    case State::WAITING_FOR_SCHEDULED_STATUS_UPDATE:
      if (this->pending_updates_.any()) {
        this->status_update_wait_credit_ms_ =
            std::min(this->update_interval_ms_, get_loop_time_ms() - this->operation_start_ms_);
        this->set_state_(State::APPLYING_SETTINGS);
        return false;
      }
      if (this->has_timed_out_(this->update_interval_ms_)) {
        this->set_state_(State::UPDATING_STATUS);
        return false;
      }
      break;

    case State::CONNECTING:
    case State::UPDATING_STATUS:
    case State::APPLYING_SETTINGS:
      if (this->has_timed_out_(RESPONSE_TIMEOUT_MS)) {
        // Some units never answer 0x06: end the chain without reconnecting, and stop asking after a few misses.
        // A dead link is still caught by the next 0x02 timeout.
        if (this->state_ == State::UPDATING_STATUS && this->current_status_msg_type_ == STATUS_MSG_OPERATING_STATUS) {
          if (++this->operating_status_misses_ >= OPERATING_STATUS_MAX_MISSES) {
            ESP_LOGI(TAG, "No operating status response, climate action stays idle");
            this->wind_down_reads_ = 0;
          }
          this->frame_parser_.reset();
          this->set_state_(State::STATUS_UPDATED);
          return this->take_status_changed_();
        }
        this->set_state_(State::READ_TIMEOUT);
        return false;
      }
      break;

    default:
      break;
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
      return from == State::DEFERRED_STATUS_REQUEST || from == State::WAITING_FOR_SCHEDULED_STATUS_UPDATE;

    case State::STATUS_UPDATED:
      return from == State::UPDATING_STATUS;

    case State::DEFERRED_STATUS_REQUEST:
      return from == State::CONNECTED || from == State::STATUS_UPDATED;

    case State::SCHEDULE_NEXT_STATUS_UPDATE:
      return from == State::STATUS_UPDATED || from == State::SETTINGS_APPLIED;

    case State::WAITING_FOR_SCHEDULED_STATUS_UPDATE:
      return from == State::SCHEDULE_NEXT_STATUS_UPDATE;

    case State::APPLYING_SETTINGS:
      return from == State::WAITING_FOR_SCHEDULED_STATUS_UPDATE;

    case State::SETTINGS_APPLIED:
      return from == State::APPLYING_SETTINGS;

    case State::READ_TIMEOUT:
      return from == State::UPDATING_STATUS || from == State::APPLYING_SETTINGS || from == State::CONNECTING;

    case State::NOT_CONNECTED:
      return false;
  }
  return false;
}

void MitsubishiCN105::did_transition_(State to) {
  switch (to) {
    case State::CONNECTING:
      this->send_packet_(CONNECT_PACKET);
      break;

    case State::CONNECTED:
      this->current_status_msg_type_ = STATUS_MSG_SETTINGS;
      this->set_state_(State::DEFERRED_STATUS_REQUEST);
      break;

    case State::UPDATING_STATUS:
      this->update_status_();
      break;

    case State::STATUS_UPDATED: {
      // When present, pending settings are applied from WAITING_FOR_SCHEDULED_STATUS_UPDATE during the next update(),
      // deferring transmission to a later loop iteration; some units might not respond if a request is sent
      // immediately after a response, causing the request to time out.
      const bool should_apply_pending_settings = this->pending_updates_.any() && this->is_status_initialized();
      const uint8_t next_msg_type = this->next_status_msg_type_();
      if (!should_apply_pending_settings && next_msg_type != STATUS_MSG_NONE && this->should_request_telemetry_()) {
        this->current_status_msg_type_ = next_msg_type;
        this->set_state_(State::DEFERRED_STATUS_REQUEST);
        break;
      }
      if (next_msg_type == STATUS_MSG_NONE) {
        // The telemetry chain completed. An early refresh keeps the regular schedule and is spent until the next
        // regular read, so a flapping setting cannot cause more than one extra read per interval.
        if (this->is_telemetry_interval_elapsed_()) {
          this->last_telemetry_update_ms_ = get_loop_time_ms();
          this->has_telemetry_update_ = true;
          this->telemetry_refresh_ = TelemetryRefresh::TELEMETRY_REFRESH_NONE;
        } else if (this->telemetry_refresh_ == TelemetryRefresh::TELEMETRY_REFRESH_REQUESTED) {
          this->telemetry_refresh_ = TelemetryRefresh::TELEMETRY_REFRESH_SPENT;
        }
      }
      this->set_state_(State::SCHEDULE_NEXT_STATUS_UPDATE);
      break;
    }

    case State::SCHEDULE_NEXT_STATUS_UPDATE:
      this->operation_start_ms_ = get_loop_time_ms() - this->status_update_wait_credit_ms_;
      this->status_update_wait_credit_ms_ = 0;
      this->current_status_msg_type_ = STATUS_MSG_SETTINGS;
      this->set_state_(State::WAITING_FOR_SCHEDULED_STATUS_UPDATE);
      break;

    case State::APPLYING_SETTINGS:
      this->apply_settings_();
      break;

    case State::SETTINGS_APPLIED:
      this->set_state_(State::SCHEDULE_NEXT_STATUS_UPDATE);
      break;

    case State::READ_TIMEOUT:
      this->frame_parser_.reset();
      this->status_update_wait_credit_ms_ = 0;
      this->set_state_(State::CONNECTING);
      break;

    case State::NOT_CONNECTED:
    case State::DEFERRED_STATUS_REQUEST:
    case State::WAITING_FOR_SCHEDULED_STATUS_UPDATE:
      break;
  }
}

bool MitsubishiCN105::should_request_telemetry_() const {
  return this->is_telemetry_polling_enabled() && (this->is_refresh_due_() || this->is_telemetry_interval_elapsed_());
}

bool MitsubishiCN105::is_telemetry_interval_elapsed_() const {
  return !this->has_telemetry_update_ ||
         (get_loop_time_ms() - this->last_telemetry_update_ms_) >= this->telemetry_request_min_interval_ms_;
}

bool MitsubishiCN105::is_early_telemetry_refresh_() const {
  return this->is_refresh_due_() && !this->is_telemetry_interval_elapsed_();
}

// An early 0x06 read after a mode or power change, or the next read while the unit winds down.
bool MitsubishiCN105::is_refresh_due_() const {
  return this->telemetry_refresh_ == TelemetryRefresh::TELEMETRY_REFRESH_REQUESTED ||
         (this->wind_down_reads_ != 0 && (get_loop_time_ms() - this->wind_down_read_ms_) >= WIND_DOWN_READ_INTERVAL_MS);
}

// Switching between AUTO and HEAT or COOL keeps the compressor running in the same direction.
static constexpr bool continues_run(MitsubishiCN105::Mode from, MitsubishiCN105::Mode to) {
  using Mode = MitsubishiCN105::Mode;
  return (from == Mode::AUTO && (to == Mode::HEAT || to == Mode::COOL)) ||
         (to == Mode::AUTO && (from == Mode::HEAT || from == Mode::COOL));
}

// The unit keeps reporting "running" while it winds down from a run that a power-off or a mode change ended.
void MitsubishiCN105::hold_if_run_ended_(bool was_on, Mode was_mode) {
  if (!this->status_.operating || !was_on || this->is_operating_status_unsupported_() ||
      (this->status_.power_on && (was_mode == this->status_.mode || continues_run(was_mode, this->status_.mode)))) {
    return;
  }
  this->status_.operating = false;
  this->wind_down_reads_ = 1;
  // The first read is due right away
  this->wind_down_read_ms_ = get_loop_time_ms() - WIND_DOWN_READ_INTERVAL_MS;
}

// A mode or power change makes the operating status stale, so read it again without waiting for the interval.
// A wind-down hold has its own reads.
void MitsubishiCN105::request_telemetry_refresh_() {
  if (this->telemetry_refresh_ == TelemetryRefresh::TELEMETRY_REFRESH_NONE &&
      !this->is_operating_status_unsupported_() && this->wind_down_reads_ == 0) {
    this->telemetry_refresh_ = TelemetryRefresh::TELEMETRY_REFRESH_REQUESTED;
  }
}

uint8_t MitsubishiCN105::next_status_msg_type_() const {
  switch (this->current_status_msg_type_) {
    case STATUS_MSG_SETTINGS:
      // An early refresh only needs the operating status.
      return this->is_early_telemetry_refresh_() ? STATUS_MSG_OPERATING_STATUS : STATUS_MSG_ROOM_TEMPERATURE;
    case STATUS_MSG_ROOM_TEMPERATURE:
      return this->is_operating_status_unsupported_() ? STATUS_MSG_NONE : STATUS_MSG_OPERATING_STATUS;
    default:
      return STATUS_MSG_NONE;
  }
}

// Changes are reported once the request chain ends, so a cycle publishes once with all of its responses applied.
bool MitsubishiCN105::take_status_changed_() {
  if (this->state_ == State::DEFERRED_STATUS_REQUEST || !this->is_status_initialized()) {
    return false;
  }
  const bool changed = this->status_changed_;
  this->status_changed_ = false;
  return changed;
}

void MitsubishiCN105::send_packet_(std::span<const uint8_t> packet) {
  FrameParser::dump_buffer_vv("TX", packet.data(), packet.size());
  this->device_.write_array(packet.data(), packet.size());
  this->operation_start_ms_ = get_loop_time_ms();
}

void MitsubishiCN105::update_status_() {
  std::array<uint8_t, REQUEST_PAYLOAD_LEN> payload{this->current_status_msg_type_};
  this->send_packet_(make_packet(PACKET_TYPE_STATUS_REQUEST, payload));
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

  const bool power_or_mode_changed = previous.power_on != this->status_.power_on || previous.mode != this->status_.mode;
  if (power_or_mode_changed) {
    this->hold_if_run_ended_(previous.power_on, previous.mode);
    this->request_telemetry_refresh_();
  }

  if (msg_type == this->current_status_msg_type_) {
    this->set_state_(State::STATUS_UPDATED);
  }

  bool changed = power_or_mode_changed || previous.fan_mode != this->status_.fan_mode ||
                 previous.target_temperature != this->status_.target_temperature ||
                 previous.vane_mode != this->status_.vane_mode ||
                 previous.wide_vane_mode != this->status_.wide_vane_mode;

  if (this->is_telemetry_polling_enabled()) {
    changed |=
        previous.room_temperature != this->status_.room_temperature || previous.operating != this->status_.operating;
  }

  this->status_changed_ |= changed;
  return this->take_status_changed_();
}

bool MitsubishiCN105::parse_status_payload_(uint8_t msg_type, const uint8_t *payload, size_t len) {
  Property::Decoder decoder{std::span{payload, len}, this->property_context_, this->pending_updates_};
  switch (msg_type) {
    case STATUS_MSG_SETTINGS:
      if (!decoder.decode_settings(this->status_)) {
        ESP_LOGVV(TAG, "RX settings payload too short");
        return false;
      }
      return true;

    case STATUS_MSG_ROOM_TEMPERATURE:
      if (!decoder.decode_room_temperature(this->status_)) {
        ESP_LOGVV(TAG, "RX room temperature payload too short");
        return false;
      }
      return true;

    case STATUS_MSG_OPERATING_STATUS:
      if (!decoder.decode_operating_status(this->status_)) {
        ESP_LOGVV(TAG, "RX operating status payload too short");
        return false;
      }
      this->operating_status_misses_ = 0;
      // The hold ends at the first "stopped" report, or after WIND_DOWN_HELD_READS reads
      if (this->wind_down_reads_ != 0 && this->status_.operating && this->wind_down_reads_ <= WIND_DOWN_HELD_READS) {
        ESP_LOGV(TAG, "Ignoring running report during wind-down");
        this->status_.operating = false;
        this->wind_down_reads_++;
        this->wind_down_read_ms_ = get_loop_time_ms();
      } else {
        this->wind_down_reads_ = 0;
      }
      return true;

    default:
      ESP_LOGVV(TAG, "RX unsupported status msg type 0x%02X", msg_type);
      return false;
  }
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
  this->pending_updates_.set(Property::Temperature::Remote::ID);
}

void MitsubishiCN105::set_power(bool power_on) {
  const bool was_on = this->status_.power_on;
  this->status_.power_on = power_on;
  this->pending_updates_.set(Property::Power::ID);
  this->hold_if_run_ended_(was_on, this->status_.mode);
}

void MitsubishiCN105::set_target_temperature(float target_temperature) {
  if (target_temperature < 16 || target_temperature > 31) {
    ESP_LOGD(TAG, "Setting temperature out-of-range: %.1f", target_temperature);
    return;
  }
  this->status_.target_temperature = target_temperature;
  this->pending_updates_.set(Property::Temperature::Target::ID);
}

void MitsubishiCN105::set_mode(Mode mode) {
  const Mode was_mode = this->status_.mode;
  if (!Property::Mode::validate_and_set(mode, this->status_, this->pending_updates_)) {
    ESP_LOGD(TAG, "Ignoring invalid mode: %u", static_cast<uint8_t>(mode));
    return;
  }
  this->hold_if_run_ended_(this->status_.power_on, was_mode);
}

void MitsubishiCN105::set_fan_mode(FanMode fan_mode) {
  if (!Property::FanMode::validate_and_set(fan_mode, this->status_, this->pending_updates_)) {
    ESP_LOGD(TAG, "Ignoring invalid fan mode: %u", static_cast<uint8_t>(fan_mode));
  }
}

void MitsubishiCN105::set_vane_mode(VaneMode vane_mode) {
  if (!Property::VaneMode::validate_and_set(vane_mode, this->status_, this->pending_updates_)) {
    ESP_LOGD(TAG, "Ignoring invalid vane mode: %u", static_cast<uint8_t>(vane_mode));
  }
}

void MitsubishiCN105::set_wide_vane_mode(WideVaneMode wide_vane_mode) {
  if (!Property::WideVaneMode::validate_and_set(wide_vane_mode, this->status_, this->pending_updates_)) {
    ESP_LOGD(TAG, "Ignoring invalid wide vane mode: %u", static_cast<uint8_t>(wide_vane_mode));
  }
}

void MitsubishiCN105::apply_settings_() {
  std::array<uint8_t, REQUEST_PAYLOAD_LEN> payload{};
  Property::Encoder encoder{payload.data(), this->property_context_, this->pending_updates_};
  if (this->pending_updates_.contains(PropertyId::POWER) || this->pending_updates_.contains(PropertyId::MODE)) {
    this->request_telemetry_refresh_();
  }

  // Apply all other pending settings first; handle REMOTE_TEMPERATURE last
  if (this->pending_updates_.contains_only(Property::Temperature::Remote::ID)) {
    encoder.encode_remote_temperature(this->remote_temperature_half_deg_);
  } else {
    encoder.encode_settings(this->status_);
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
    case State::DEFERRED_STATUS_REQUEST:
      return LOG_STR("DeferredStatusRequest");
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
  ESP_LOGVV_HEX(TAG, data, len, READ_BUFFER_SIZE, "%s (%zu): %s", prefix, len);
}

}  // namespace esphome::mitsubishi_cn105
