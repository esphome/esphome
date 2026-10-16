#include "hon_water_heater.h"

#include <cmath>

#include "esphome/core/application.h"
#ifdef USE_NETWORK
#include "esphome/components/network/util.h"
#endif
#include "esphome/core/log.h"
#include "esphome/core/progmem.h"

namespace esphome::haier_water_heater {

ESPHOME_LOG_TAG(TAG, "haier_water_heater");
using namespace water_heater_protocol;

// Indexed by HonMode, with Unknown as the fallback for out-of-range values.
PROGMEM_STRING_TABLE(HonModeLabels, "Unknown", "Normal", "ECO", "BPS");

void HonWaterHeater::setup() {
  this->status_set_warning(LOG_STR("Waiting for a valid appliance status frame"));
  if (this->communication_sensor_ != nullptr)
    this->communication_sensor_->publish_state(false);
  this->update_network_status_();
  this->set_interval(NETWORK_STATUS_INTERVAL_ID, NETWORK_STATUS_INTERVAL_MS,
                     [this]() { this->update_network_status_(); });
  this->set_interval(STATUS_POLL_INTERVAL_ID, POLL_INTERVAL_MS, [this]() { this->poll_status_(); });
}

void HonWaterHeater::update_network_status_() {
#ifdef USE_NETWORK
  const bool ready = network::is_connected();
#else
  const bool ready = false;
#endif
  const uint8_t status = ready ? NETWORK_READY : NETWORK_NOT_READY;
  if (status != this->network_status_) {
    this->network_status_ = status;
    this->network_pending_ = true;
    ESP_LOGI(TAG, "Local network status changed: %02X", status);
  }
}

void HonWaterHeater::send_frame_(FrameType type, const uint8_t *payload, size_t size) {
  uint8_t wire[MAX_WIRE_FRAME];
  const size_t length = encode_frame(type, payload, size, wire);
  if (length == 0)
    return;
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex[format_hex_pretty_size(sizeof(wire))];
  ESP_LOGV(TAG, "TX type=%02X: %s", static_cast<uint8_t>(type), format_hex_pretty_to(hex, wire, length));
#endif
  this->write_array(wire, length);
}

void HonWaterHeater::send_request_(uint32_t now, Awaiting kind, FrameType type, const uint8_t *payload, size_t size,
                                   FrameType expected) {
  this->send_frame_(type, payload, size);
  this->awaiting_ = kind;
  this->expected_type_ = expected;
  this->request_sent_ = now;
}

void HonWaterHeater::poll_status_() {
  const uint32_t now = App.get_loop_component_start_time();
  this->check_timeouts_(now);
  if (this->awaiting_ != Awaiting::AWAITING_NONE || this->command_count_ != 0 || this->pending_off_)
    return;
  if (this->network_pending_ || now - this->network_sent_ >= NETWORK_REFRESH_MS)
    return;  // the network status goes out first, from loop()
  // The version and device id are read once per cycle, the status on every other pass.
  static constexpr uint8_t VERSION_REQUEST[] = {0x00, 0x07};
  static constexpr uint8_t STATUS_REQUEST[] = {0x4D, 0x01};
  if (this->poll_stage_ == 0) {
    this->send_request_(now, Awaiting::AWAITING_POLL, FrameType::FRAME_TYPE_GET_VERSION, VERSION_REQUEST,
                        sizeof(VERSION_REQUEST), FrameType::FRAME_TYPE_VERSION);
  } else if (this->poll_stage_ == 1) {
    this->send_request_(now, Awaiting::AWAITING_POLL, FrameType::FRAME_TYPE_GET_DEVICE_ID, nullptr, 0,
                        FrameType::FRAME_TYPE_DEVICE_ID);
  } else {
    this->send_request_(now, Awaiting::AWAITING_POLL, FrameType::FRAME_TYPE_CONTROL, STATUS_REQUEST,
                        sizeof(STATUS_REQUEST), FrameType::FRAME_TYPE_STATUS);
  }
  this->poll_stage_ = (this->poll_stage_ + 1) % POLL_CYCLE;
}

void HonWaterHeater::check_timeouts_(uint32_t now) {
  if (this->have_status_ && now - this->last_status_ >= this->communication_timeout_) {
    this->have_status_ = false;
    this->status_set_warning(LOG_STR("Heater communication timed out"));
    ESP_LOGW(TAG, "Heater communication lost: no valid status for %u ms",
             static_cast<unsigned>(this->communication_timeout_));
    if (this->communication_sensor_ != nullptr)
      this->communication_sensor_->publish_state(false);
  }
  const uint32_t waited = now - this->request_sent_;
  switch (this->awaiting_) {
    case Awaiting::AWAITING_NONE:
      break;
    case Awaiting::AWAITING_POLL:
      if (waited >= RESPONSE_TIMEOUT_MS) {
        ESP_LOGW(TAG, "No matching response type=%02X in %u ms", static_cast<uint8_t>(this->expected_type_),
                 static_cast<unsigned>(RESPONSE_TIMEOUT_MS));
        this->awaiting_ = Awaiting::AWAITING_NONE;
      }
      break;
    case Awaiting::AWAITING_CONTROL:
      if (waited >= RESPONSE_TIMEOUT_MS) {
        ESP_LOGW(TAG, "Control response timed out; discarding remaining commands");
        this->awaiting_ = Awaiting::AWAITING_NONE;
        this->command_count_ = 0;
      }
      break;
    case Awaiting::AWAITING_NETWORK:
      if (waited >= NETWORK_ACK_TIMEOUT_MS) {
        ESP_LOGW(TAG, "Network status: no CONFIRM within %u ms", static_cast<unsigned>(NETWORK_ACK_TIMEOUT_MS));
        this->awaiting_ = Awaiting::AWAITING_NONE;
      }
      break;
  }
}

void HonWaterHeater::send_pending_(uint32_t now) {
  if (this->awaiting_ != Awaiting::AWAITING_NONE)
    return;
  if (this->pending_off_) {
    this->commands_[0] = {Parameter::PARAMETER_POWER, 0};
    this->command_index_ = 0;
    this->command_count_ = 1;
    this->pending_off_ = false;
  }
  if (this->command_count_ == 0 && (this->network_pending_ || now - this->network_sent_ >= NETWORK_REFRESH_MS)) {
    // Signal remains a placeholder; status represents local IP readiness, not cloud access.
    const uint8_t payload[] = {0x00, this->network_status_, 0x00, 100};
    this->send_request_(now, Awaiting::AWAITING_NETWORK, FrameType::FRAME_TYPE_NETWORK_STATUS, payload, sizeof(payload),
                        FrameType::FRAME_TYPE_CONFIRM);
    this->network_sent_ = now;
    this->network_pending_ = false;
    return;
  }
  if (this->command_count_ != 0)
    this->send_next_control_(now);
}

void HonWaterHeater::send_next_control_(uint32_t now) {
  uint8_t payload[4];
  encode_control(this->commands_[this->command_index_], payload);
  this->send_request_(now, Awaiting::AWAITING_CONTROL, FrameType::FRAME_TYPE_CONTROL, payload, sizeof(payload),
                      FrameType::FRAME_TYPE_STATUS);
}

void HonWaterHeater::read_frames_(uint32_t now) {
  if (now - this->last_byte_ > FRAME_GAP_MS)
    this->parser_.reset();
  uint8_t budget = MAX_BYTES_PER_LOOP;
  while (budget-- > 0 && this->available() > 0) {
    uint8_t value;
    if (!this->read_byte(&value))
      break;
    this->last_byte_ = now;
    if (this->parser_.feed(value)) {
      this->process_frame_();
    } else if (this->parser_.complete()) {
      if ((this->parser_.data()[FRAME_FLAGS_OFFSET] & FRAME_FLAG_ENCRYPTED) != 0) {
        ESP_LOGW(TAG, "RX: XOR-encrypted frames are not supported and will be dropped");
      } else {
        ESP_LOGW(TAG, "RX: unsupported flags or invalid checksum/CRC");
      }
    }
  }
}

void HonWaterHeater::loop() {
  const uint32_t now = App.get_loop_component_start_time();
  this->check_timeouts_(now);
  this->send_pending_(now);
  this->read_frames_(now);
}

void HonWaterHeater::handle_frame_type_(FrameType type) {
  if (this->awaiting_ == Awaiting::AWAITING_POLL && type == this->expected_type_) {
    ESP_LOGV(TAG, "Response matched type=%02X", static_cast<uint8_t>(type));
    this->awaiting_ = Awaiting::AWAITING_NONE;
  }
  switch (type) {
    case FrameType::FRAME_TYPE_GET_NETWORK_STATUS:
      this->network_pending_ = true;
      break;
    case FrameType::FRAME_TYPE_CONFIRM:
      if (this->awaiting_ == Awaiting::AWAITING_NETWORK) {
        ESP_LOGD(TAG, "Network status acknowledged");
        this->awaiting_ = Awaiting::AWAITING_NONE;
      }
      break;
    case FrameType::FRAME_TYPE_INVALID:
      ESP_LOGW(TAG, "Appliance rejected request");
      // Controls queued behind a rejected poll were never sent and keep their turn.
      if (this->awaiting_ == Awaiting::AWAITING_CONTROL)
        this->command_count_ = 0;
      if (this->awaiting_ != Awaiting::AWAITING_POLL)
        this->awaiting_ = Awaiting::AWAITING_NONE;
      break;
    case FrameType::FRAME_TYPE_REPORT:
      this->send_frame_(FrameType::FRAME_TYPE_CONFIRM);
      break;
    default:
      break;
  }
}

void HonWaterHeater::confirm_control_(const Status &status) {
  const Command &command = this->commands_[this->command_index_];
  if (!command_confirmed(command, status))
    return;
  ESP_LOGD(TAG, "Control confirmed: parameter=%02X value=%02X", static_cast<uint8_t>(command.parameter), command.value);
  this->awaiting_ = Awaiting::AWAITING_NONE;
  if (++this->command_index_ == this->command_count_)
    this->command_count_ = 0;
}

void HonWaterHeater::process_frame_() {
  const uint8_t *frame = this->parser_.data();
  const auto type = static_cast<FrameType>(frame[FRAME_TYPE_OFFSET]);
#if ESPHOME_LOG_LEVEL >= ESPHOME_LOG_LEVEL_VERBOSE
  char hex[format_hex_pretty_size(MAX_STORED_FRAME)];
  ESP_LOGV(TAG, "RX decoded type=%02X flags=%02X: %s", static_cast<uint8_t>(type), frame[FRAME_FLAGS_OFFSET],
           format_hex_pretty_to(hex, frame, this->parser_.stored_size()));
#endif
  this->handle_frame_type_(type);

  Status status{};
  if (!decode_status(frame, this->parser_.size(), status))
    return;
  if (!this->have_status_) {
    ESP_LOGI(TAG, "Heater communication established");
    if (this->communication_sensor_ != nullptr)
      this->communication_sensor_->publish_state(true);
  }
  if (status.mode == HonMode::HON_MODE_UNKNOWN &&
      (!this->have_status_ || this->reported_mode_ != HonMode::HON_MODE_UNKNOWN)) {
    const uint8_t *payload = frame + FRAME_PAYLOAD_OFFSET;
    ESP_LOGW(TAG,
             "Unknown heater mode: payload[38..41]=%02X %02X %02X %02X; using last recognized active mode, or Electric "
             "if none is known",
             payload[38], payload[39], payload[40], payload[41]);
  }
  this->have_status_ = true;
  this->last_status_ = App.get_loop_component_start_time();
  this->reported_mode_ = status.mode;
  if (this->awaiting_ == Awaiting::AWAITING_CONTROL && type == FrameType::FRAME_TYPE_STATUS)
    this->confirm_control_(status);
  this->status_clear_warning();
  this->publish_status_(status);
}

void HonWaterHeater::publish_status_(const Status &status) {
  this->current_temperature_ = status.current_temperature;
  this->target_temperature_ = status.target_temperature;
  this->set_state_flag_(water_heater::WATER_HEATER_STATE_ON, status.powered);
  switch (status.mode) {
    case HonMode::HON_MODE_NORMAL:
      this->last_active_mode_ = water_heater::WATER_HEATER_MODE_ELECTRIC;
      break;
    case HonMode::HON_MODE_ECO:
      this->last_active_mode_ = water_heater::WATER_HEATER_MODE_ECO;
      break;
    case HonMode::HON_MODE_BPS:
      this->last_active_mode_ = water_heater::WATER_HEATER_MODE_PERFORMANCE;
      break;
    case HonMode::HON_MODE_UNKNOWN:
      break;
  }
  // The base entity has no Unknown mode; the text sensor preserves the decoded result.
  this->mode_ = status.powered ? this->last_active_mode_ : water_heater::WATER_HEATER_MODE_OFF;
  if (this->primary_sensor_ != nullptr)
    this->primary_sensor_->publish_state(status.current_temperature);
  if (this->secondary_sensor_ != nullptr) {
    this->secondary_sensor_->publish_state(
        status.secondary_temperature <= MAX_REPORTED_TEMPERATURE ? status.secondary_temperature : NAN);
  }
  if (this->target_sensor_ != nullptr)
    this->target_sensor_->publish_state(status.target_temperature);
  if (this->heating_time_sensor_ != nullptr)
    this->heating_time_sensor_->publish_state(status.estimated_heating_time);
  if (this->water_sensor_ != nullptr) {
    // Three segments of available hot water, published as 0, 33, 66 or 100 percent.
    const uint8_t percent = status.available_water * 100 / 3;
    this->water_sensor_->publish_state(status.available_water <= 3 ? percent : NAN);
  }
  if (this->power_sensor_ != nullptr)
    this->power_sensor_->publish_state(status.powered);
  if (this->heating_sensor_ != nullptr)
    this->heating_sensor_->publish_state(status.heating);
  if (this->mode_sensor_ != nullptr) {
    char label[HonModeLabels::MAX_LENGTH + 1];
    ESPHOME_strncpy_P(label,
                      reinterpret_cast<ESPHOME_PGM_P>(HonModeLabels::get_progmem_str(
                          static_cast<uint8_t>(status.mode), static_cast<uint8_t>(HonMode::HON_MODE_UNKNOWN))),
                      sizeof(label));
    this->mode_sensor_->publish_state(label);
  }
  this->publish_state();
}

void HonWaterHeater::dump_config() {
  LOG_WATER_HEATER("", "Haier hOn", this);
  ESP_LOGCONFIG(TAG,
                "  Polling interval: %u ms\n"
                "  Communication timeout: %u ms",
                static_cast<unsigned>(POLL_INTERVAL_MS), static_cast<unsigned>(this->communication_timeout_));
  LOG_SENSOR("  ", "Primary temperature", this->primary_sensor_);
  LOG_SENSOR("  ", "Secondary temperature", this->secondary_sensor_);
  LOG_SENSOR("  ", "Target temperature", this->target_sensor_);
  LOG_SENSOR("  ", "Estimated heating time", this->heating_time_sensor_);
  LOG_SENSOR("  ", "Available water", this->water_sensor_);
  LOG_BINARY_SENSOR("  ", "Communication", this->communication_sensor_);
  LOG_BINARY_SENSOR("  ", "Power", this->power_sensor_);
  LOG_BINARY_SENSOR("  ", "Heating", this->heating_sensor_);
  LOG_TEXT_SENSOR("  ", "Mode", this->mode_sensor_);
}

water_heater::WaterHeaterCallInternal HonWaterHeater::make_call() {
  return water_heater::WaterHeaterCallInternal(this);
}

// The appliance mode a water heater mode maps to; Unknown for modes the appliance does not have.
static HonMode hon_mode(water_heater::WaterHeaterMode mode) {
  switch (mode) {
    case water_heater::WATER_HEATER_MODE_ELECTRIC:
      return HonMode::HON_MODE_NORMAL;
    case water_heater::WATER_HEATER_MODE_ECO:
      return HonMode::HON_MODE_ECO;
    case water_heater::WATER_HEATER_MODE_PERFORMANCE:
      return HonMode::HON_MODE_BPS;
    default:
      return HonMode::HON_MODE_UNKNOWN;
  }
}

void HonWaterHeater::control(const water_heater::WaterHeaterCall &call) {
  using namespace water_heater;
  const auto mode = call.get_mode();
  const auto on = call.get_on();
  const bool off = (on.has_value() && !*on) || (mode.has_value() && *mode == WATER_HEATER_MODE_OFF);
  if (off) {
    // Keep only the command in flight until its reply or timeout; unsent writes are dropped.
    if (this->awaiting_ == Awaiting::AWAITING_CONTROL) {
      this->commands_[0] = this->commands_[this->command_index_];
      this->command_index_ = 0;
      this->command_count_ = 1;
    } else {
      this->command_count_ = 0;
    }
    this->pending_off_ = true;
    return;
  }
  if (!this->have_status_ || App.get_loop_component_start_time() - this->last_status_ > CONTROL_STATUS_MAX_AGE_MS) {
    ESP_LOGW(TAG, "Control rejected: no recent appliance status");
    return;
  }
  if (this->command_count_ != 0 || this->pending_off_) {
    ESP_LOGW(TAG, "Control busy; wait for the previous request to finish");
    return;
  }
  const float temperature = call.get_target_temperature();
  if (!std::isnan(temperature)) {
    if (!std::isfinite(temperature) || temperature < MIN_TARGET_TEMPERATURE || temperature > MAX_TARGET_TEMPERATURE ||
        std::round(temperature) != temperature) {
      ESP_LOGW(TAG, "Target must be a whole temperature from %u to %u C", MIN_TARGET_TEMPERATURE,
               MAX_TARGET_TEMPERATURE);
      return;
    }
    const bool normal =
        mode.has_value() ? *mode == WATER_HEATER_MODE_ELECTRIC : this->reported_mode_ == HonMode::HON_MODE_NORMAL;
    if (!normal) {
      ESP_LOGW(TAG, "Target can only be set in Normal mode");
      return;
    }
  }
  this->command_index_ = 0;
  auto append = [this](Parameter parameter, uint8_t value) {
    this->commands_[this->command_count_++] = {parameter, value};
  };
  // A write is only queued when it changes something, so a repeated state costs no round trip.
  if (!this->is_on() && ((on.has_value() && *on) || mode.has_value()))
    append(Parameter::PARAMETER_POWER, 1);
  if (mode.has_value() && hon_mode(*mode) != this->reported_mode_) {
    // Captures show the appliance moves between ECO and BPS through Normal, never directly.
    if (*mode != WATER_HEATER_MODE_ELECTRIC && this->reported_mode_ != HonMode::HON_MODE_NORMAL)
      append(Parameter::PARAMETER_ECO, 0);
    switch (*mode) {
      case WATER_HEATER_MODE_ELECTRIC:
        append(Parameter::PARAMETER_ECO, 0);
        break;
      case WATER_HEATER_MODE_ECO:
        append(Parameter::PARAMETER_ECO, 1);
        break;
      case WATER_HEATER_MODE_PERFORMANCE:
        append(Parameter::PARAMETER_BPS, 1);
        break;
      default:
        break;
    }
  }
  if (!std::isnan(temperature))
    append(Parameter::PARAMETER_TARGET_TEMPERATURE, static_cast<uint8_t>(temperature - TARGET_TEMPERATURE_OFFSET));
}

water_heater::WaterHeaterTraits HonWaterHeater::traits() {
  using namespace water_heater;
  WaterHeaterTraits traits;
  traits.set_supports_current_temperature(true);
  traits.add_feature_flags(WATER_HEATER_SUPPORTS_TARGET_TEMPERATURE | WATER_HEATER_SUPPORTS_OPERATION_MODE |
                           WATER_HEATER_SUPPORTS_ON_OFF);
  traits.set_supported_modes(
      {WATER_HEATER_MODE_OFF, WATER_HEATER_MODE_ELECTRIC, WATER_HEATER_MODE_ECO, WATER_HEATER_MODE_PERFORMANCE});
  traits.set_min_temperature(MIN_TARGET_TEMPERATURE);
  traits.set_max_temperature(MAX_TARGET_TEMPERATURE);
  traits.set_target_temperature_step(1);
  return traits;
}

}  // namespace esphome::haier_water_heater
