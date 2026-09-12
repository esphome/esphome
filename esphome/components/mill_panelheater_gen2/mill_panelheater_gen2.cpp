#include "mill_panelheater_gen2.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

namespace esphome::mill_panelheater_gen2 {

static const char *const TAG = "mill_panelheater_gen2.climate";

void MillPanelHeaterGen2::setup() {
  ESP_LOGD(TAG, "MillPanelHeaterGen2 initialization...");
  this->reset_communication_timeout_();
}

void MillPanelHeaterGen2::dump_config() {
  ESP_LOGCONFIG(TAG, "MillPanelHeaterGen2:");
  LOG_CLIMATE("", "MillPanelHeaterGen2 Climate", this);
  LOG_SENSOR("  ", "Estimated Power", this->power_sensor_);
  if (this->power_sensor_ != nullptr) {
    ESP_LOGCONFIG(TAG, "  Rated power: %.0f W", this->rated_power_);
  }
  this->check_uart_settings(9600);
}

void MillPanelHeaterGen2::loop() {
  this->receive_byte_();

  if (!this->new_data_) {
    return;
  }
  this->new_data_ = false;

  if (this->received_length_ <= COMMAND_TYPE_POS) {
    ESP_LOGD(TAG, "Ignoring short frame: payload has %u bytes; command type is unavailable",
             static_cast<unsigned>(this->received_length_));
    return;
  }

  if (this->received_data_[COMMAND_TYPE_POS] != STATUS_COMMAND_TYPE) {
    ESP_LOGV(TAG, "Ignoring frame: type 0x%02X is not status type 0x%02X", this->received_data_[COMMAND_TYPE_POS],
             STATUS_COMMAND_TYPE);
    return;
  }

  if (this->received_data_[FRAME_LENGTH_POS] != STATUS_FRAME_LENGTH ||
      this->received_length_ != STATUS_FRAME_LENGTH - FRAME_OVERHEAD_SIZE) {
    ESP_LOGW(TAG, "Rejecting C9 status frame: declared length=%u, payload length=%u; expected %u and %u",
             this->received_data_[FRAME_LENGTH_POS], static_cast<unsigned>(this->received_length_),
             static_cast<unsigned>(STATUS_FRAME_LENGTH),
             static_cast<unsigned>(STATUS_FRAME_LENGTH - FRAME_OVERHEAD_SIZE));
    return;
  }

  const uint8_t expected_checksum = this->received_data_[this->received_length_ - 1];
  const uint8_t calculated_checksum = checksum(this->received_data_.data(), this->received_length_ - 1);
  if (expected_checksum != calculated_checksum) {
    ESP_LOGW(TAG, "Rejecting C9 status frame: checksum 0x%02X does not match calculated checksum 0x%02X",
             expected_checksum, calculated_checksum);
    return;
  }

  const uint8_t raw_target_temperature = this->received_data_[TARGET_TEMP_POS];
  const uint8_t raw_current_temperature = this->received_data_[CURRENT_TEMP_POS];
  const uint8_t raw_mode = this->received_data_[MODE_POS];
  const uint8_t raw_action = this->received_data_[ACTION_POS];
  if (raw_target_temperature < MIN_TARGET_TEMPERATURE || raw_target_temperature > MAX_TARGET_TEMPERATURE) {
    ESP_LOGW(TAG, "Rejecting C9 status frame: target temperature %u is outside supported range %u-%u",
             static_cast<unsigned>(raw_target_temperature), static_cast<unsigned>(MIN_TARGET_TEMPERATURE),
             static_cast<unsigned>(MAX_TARGET_TEMPERATURE));
    return;
  }
  if (raw_mode != PROTOCOL_MODE_OFF && raw_mode != PROTOCOL_MODE_HEAT) {
    ESP_LOGW(TAG, "Rejecting C9 status frame: unsupported mode value 0x%02X", raw_mode);
    return;
  }
  if (raw_action != PROTOCOL_ACTION_IDLE && raw_action != PROTOCOL_ACTION_HEATING) {
    ESP_LOGW(TAG, "Rejecting C9 status frame: unsupported action value 0x%02X", static_cast<unsigned>(raw_action));
    return;
  }

  this->reset_communication_timeout_();
  this->status_clear_warning();

  this->target_temperature = raw_target_temperature;

  this->current_temperature = raw_current_temperature;

  if (raw_mode == PROTOCOL_MODE_OFF) {
    this->mode = climate::CLIMATE_MODE_OFF;
    this->action = climate::CLIMATE_ACTION_OFF;
  } else {
    this->mode = climate::CLIMATE_MODE_HEAT;
    this->action = raw_action == PROTOCOL_ACTION_IDLE ? climate::CLIMATE_ACTION_IDLE : climate::CLIMATE_ACTION_HEATING;
  }

  ESP_LOGD(TAG, "C9 status: target=%.1f C, current=%.1f C (raw=%u), mode=%s, action=%s", this->target_temperature,
           this->current_temperature, static_cast<unsigned>(raw_current_temperature),
           LOG_STR_ARG(climate::climate_mode_to_string(this->mode)),
           LOG_STR_ARG(climate::climate_action_to_string(this->action)));
  this->publish_state();
  // Keep periodic power samples: integration sensors use each source update as an integration timestamp.
  this->publish_power_state_();
}

void MillPanelHeaterGen2::publish_power_state_() {
  if (this->power_sensor_ == nullptr) {
    return;
  }

  const float power = this->action == climate::CLIMATE_ACTION_HEATING ? this->rated_power_ : 0.0f;
  this->power_sensor_->publish_state(power);
}

void MillPanelHeaterGen2::reset_communication_timeout_() {
  this->set_timeout("communication_timeout", COMMUNICATION_TIMEOUT_MS, [this]() {
    ESP_LOGW(TAG, "Communication timeout: no C9 frame received for %u seconds",
             static_cast<unsigned>(COMMUNICATION_TIMEOUT_MS / 1000));
    this->status_set_warning("Communication timeout");
    this->current_temperature = NAN;
    this->target_temperature = NAN;
    this->publish_state();
    if (this->power_sensor_ != nullptr) {
      this->power_sensor_->publish_state(NAN);
    }
  });
}

void MillPanelHeaterGen2::receive_byte_() {
  if (this->available() == 0) {
    return;
  }

  uint8_t byte;
  if (!this->read_byte(&byte)) {
    return;
  }

  ESP_LOGVV(TAG, "RX byte: byte=0x%02X, receive_in_progress=%s, buffer_length=%u", byte,
            YESNO(this->receive_in_progress_), static_cast<unsigned>(this->received_length_));

  const uint32_t now = millis();
  if (this->receive_in_progress_ && now - this->last_receive_byte_time_ > RECEIVE_TIMEOUT_MS) {
    ESP_LOGD(TAG, "Discarding incomplete frame after receive timeout: payload_length=%u, expected_payload_length=%u",
             static_cast<unsigned>(this->received_length_), static_cast<unsigned>(this->expected_payload_length_));
    this->reset_receive_state_();
  }

  if (!this->receive_in_progress_) {
    if (byte == START_MARKER) {
      this->start_receive_frame_();
    }
    return;
  }

  this->last_receive_byte_time_ = now;

  if (this->expected_payload_length_ != 0 && this->received_length_ == this->expected_payload_length_) {
    if (byte != END_MARKER) {
      ESP_LOGD(TAG, "Rejecting frame with invalid final byte 0x%02X", byte);
      this->log_frame_("Rejecting frame with invalid final byte", byte);
      this->reset_receive_state_();
      if (byte == START_MARKER) {
        this->start_receive_frame_();
      }
      return;
    }

    this->log_frame_("Received length-complete frame", byte);
    this->receive_in_progress_ = false;
    this->new_data_ = true;
    return;
  }

  if (this->received_length_ >= this->received_data_.size()) {
    this->log_frame_("Rejecting overlong frame", byte);
    ESP_LOGW(TAG, "Rejecting frame: payload exceeds %u-byte receive buffer; overflow byte is 0x%02X",
             static_cast<unsigned>(this->received_data_.size()), byte);
    this->reset_receive_state_();
    return;
  }

  this->received_data_[this->received_length_++] = byte;

  if (this->received_length_ == FRAME_LENGTH_POS + 1) {
    const size_t declared_frame_length = this->received_data_[FRAME_LENGTH_POS];
    if (declared_frame_length < MIN_FRAME_LENGTH ||
        declared_frame_length > this->received_data_.size() + FRAME_OVERHEAD_SIZE) {
      ESP_LOGW(TAG, "Rejecting frame with invalid declared length %u", static_cast<unsigned>(declared_frame_length));
      this->reset_receive_state_();
      return;
    }
    this->expected_payload_length_ = declared_frame_length - FRAME_OVERHEAD_SIZE;
  }
}

void MillPanelHeaterGen2::reset_receive_state_() {
  this->received_length_ = 0;
  this->expected_payload_length_ = 0;
  this->receive_in_progress_ = false;
  this->new_data_ = false;
}

void MillPanelHeaterGen2::start_receive_frame_() {
  this->received_length_ = 0;
  this->expected_payload_length_ = 0;
  this->receive_in_progress_ = true;
  this->new_data_ = false;
  this->last_receive_byte_time_ = millis();
}

void MillPanelHeaterGen2::log_frame_(const char *message, uint8_t last_byte) const {
#ifdef ESPHOME_LOG_HAS_VERBOSE
  std::array<uint8_t, RECEIVE_BUFFER_SIZE + 2> frame{};
  char hex_buffer[format_hex_pretty_size(RECEIVE_BUFFER_SIZE + 2)];
  size_t frame_length = 0;
  frame[frame_length++] = START_MARKER;
  for (size_t i = 0; i < this->received_length_; i++) {
    frame[frame_length++] = this->received_data_[i];
  }
  frame[frame_length++] = last_byte;

  if (this->received_length_ > COMMAND_TYPE_POS) {
    ESP_LOGV(TAG, "%s: bytes=%s, length=%u, payload_length=%u, type=0x%02X, last_byte=0x%02X", message,
             format_hex_pretty_to(hex_buffer, frame.data(), frame_length, '.'), static_cast<unsigned>(frame_length),
             static_cast<unsigned>(this->received_length_), this->received_data_[COMMAND_TYPE_POS], last_byte);
  } else {
    ESP_LOGV(TAG, "%s: bytes=%s, length=%u, payload_length=%u, type=unavailable, last_byte=0x%02X", message,
             format_hex_pretty_to(hex_buffer, frame.data(), frame_length, '.'), static_cast<unsigned>(frame_length),
             static_cast<unsigned>(this->received_length_), last_byte);
  }
#else
  (void) message;
  (void) last_byte;
#endif
}

climate::ClimateTraits MillPanelHeaterGen2::traits() {
  climate::ClimateTraits traits;
  traits.set_visual_target_temperature_step(1);
  traits.set_visual_current_temperature_step(1);
  traits.set_visual_min_temperature(MIN_TARGET_TEMPERATURE);
  traits.set_visual_max_temperature(MAX_TARGET_TEMPERATURE);
  traits.add_feature_flags(climate::CLIMATE_SUPPORTS_CURRENT_TEMPERATURE | climate::CLIMATE_SUPPORTS_ACTION);
  traits.set_supported_modes({
      climate::CLIMATE_MODE_OFF,
      climate::CLIMATE_MODE_HEAT,
  });
  return traits;
}

void MillPanelHeaterGen2::control(const climate::ClimateCall &call) {
  const auto requested_mode = call.get_mode();
  const auto requested_target_temperature = call.get_target_temperature();
  ESP_LOGD(TAG, "control() called: mode_set=%s, target_temperature_set=%s", YESNO(requested_mode.has_value()),
           YESNO(requested_target_temperature.has_value()));
  if (requested_mode.has_value()) {
    ESP_LOGD(TAG, "control() requested mode=%s", LOG_STR_ARG(climate::climate_mode_to_string(*requested_mode)));
  }
  if (requested_target_temperature.has_value()) {
    ESP_LOGD(TAG, "control() requested target_temperature=%.1f", *requested_target_temperature);
  }

  const bool target_temperature_is_valid =
      !requested_target_temperature.has_value() ||
      (std::isfinite(*requested_target_temperature) && *requested_target_temperature >= MIN_TARGET_TEMPERATURE &&
       *requested_target_temperature <= MAX_TARGET_TEMPERATURE);
  if (!target_temperature_is_valid) {
    ESP_LOGW(TAG, "Ignoring target temperature %.1f: supported range is %u-%u C", *requested_target_temperature,
             static_cast<unsigned>(MIN_TARGET_TEMPERATURE), static_cast<unsigned>(MAX_TARGET_TEMPERATURE));
  }

  if (requested_mode.has_value()) {
    switch (*requested_mode) {
      case climate::CLIMATE_MODE_OFF:
        this->send_power_command_(PROTOCOL_MODE_OFF);
        ESP_LOGD(TAG, "Mode command sent; awaiting C9 status confirmation");
        break;
      case climate::CLIMATE_MODE_HEAT:
        this->send_power_command_(PROTOCOL_MODE_HEAT);
        ESP_LOGD(TAG, "Mode command sent; awaiting C9 status confirmation");
        break;
      default:
        ESP_LOGW(TAG, "Ignoring unsupported mode request");
        break;
    }
  }

  if (requested_target_temperature.has_value() && target_temperature_is_valid) {
    const auto temperature = static_cast<uint8_t>(roundf(*requested_target_temperature));
    this->send_temperature_command_(temperature);
    ESP_LOGD(TAG, "Temperature command sent; awaiting C9 status confirmation");
  }
}

void MillPanelHeaterGen2::send_power_command_(uint8_t mode_value) {
  static constexpr std::array<uint8_t, COMMAND_PAYLOAD_SIZE> PAYLOAD{
      0x00, 0x10, 0x06, 0x00, 0x47, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  this->send_command_(PAYLOAD, POWER_COMMAND_VALUE_POS, mode_value);
}

void MillPanelHeaterGen2::send_temperature_command_(uint8_t temperature) {
  static constexpr std::array<uint8_t, COMMAND_PAYLOAD_SIZE> PAYLOAD{
      0x00, 0x10, 0x22, 0x00, 0x46, 0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00,
  };
  this->send_command_(PAYLOAD, TEMPERATURE_COMMAND_VALUE_POS, temperature);
}

void MillPanelHeaterGen2::send_command_(std::array<uint8_t, COMMAND_PAYLOAD_SIZE> payload, size_t value_position,
                                        uint8_t value) {
  ESP_LOGV(TAG, "Sending serial command");
  payload[value_position] = value;

  std::array<uint8_t, COMMAND_PAYLOAD_SIZE + 3> frame{};
  frame[0] = START_MARKER;
  for (size_t i = 0; i < payload.size(); i++) {
    frame[i + 1] = payload[i];
  }
  frame[COMMAND_PAYLOAD_SIZE + 1] = checksum(payload.data(), payload.size());
  frame[COMMAND_PAYLOAD_SIZE + 2] = END_MARKER;
  this->write_array(frame);
}

uint8_t MillPanelHeaterGen2::checksum(const uint8_t *data, size_t length) {
  uint8_t checksum = 0;
  for (size_t i = 0; i < length; i++) {
    checksum += data[i];
  }
  return checksum;
}

}  // namespace esphome::mill_panelheater_gen2
