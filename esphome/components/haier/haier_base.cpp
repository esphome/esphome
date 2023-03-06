#include <chrono>
#include <string>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "haier_base.h"

using namespace esphome::climate;
using namespace esphome::uart;

#ifndef ESPHOME_LOG_LEVEL
#warning "No ESPHOME_LOG_LEVEL defined!"
#endif

namespace esphome {
namespace haier {

const char TAG[] = "haier.climate";
constexpr size_t COMMUNICATION_TIMEOUT_MS = 60000;
constexpr size_t STATUS_REQUEST_INTERVAL_MS = 5000;
constexpr size_t PROTOCOL_INITIALIZATION_INTERVAL = 10000;
constexpr size_t DEFAULT_MESSAGES_INTERVAL_MS = 2000;
constexpr size_t CONTROL_MESSAGES_INTERVAL_MS = 400;
constexpr size_t CONTROL_TIMEOUT_MS = 7000;
constexpr size_t NO_COMMAND = 0xFF;  // Indicate that there is no command supplied

#if (HAIER_LOG_LEVEL > 4)
// To reduce size of binary this function only available when log level is Verbose
const char *HaierClimateBase::phase_to_string_(ProtocolPhases phase) {
  static const char *phase_names[] = {
      "SENDING_INIT_1",
      "WAITING_ANSWER_INIT_1",
      "SENDING_INIT_2",
      "WAITING_ANSWER_INIT_2",
      "SENDING_FIRST_STATUS_REQUEST",
      "WAITING_FIRST_STATUS_ANSWER",
      "SENDING_ALARM_STATUS_REQUEST",
      "WAITING_ALARM_STATUS_ANSWER",
      "IDLE",
      "SENDING_STATUS_REQUEST",
      "WAITING_STATUS_ANSWER",
      "SENDING_UPDATE_SIGNAL_REQUEST",
      "WAITING_UPDATE_SIGNAL_ANSWER",
      "SENDING_SIGNAL_LEVEL",
      "WAITING_SIGNAL_LEVEL_ANSWER",
      "SENDING_CONTROL",
      "WAITING_CONTROL_ANSWER",
      "UNKNOWN"  // Should be the last!
  };
  int phase_index = (int) phase;
  if ((phase_index > (int) ProtocolPhases::NUM_PROTOCOL_PHASES) || (phase_index < 0))
    phase_index = (int) ProtocolPhases::NUM_PROTOCOL_PHASES;
  return phase_names[phase_index];
}
#endif

HaierClimateBase::HaierClimateBase(UARTComponent *parent)
    : UARTDevice(parent),
      haier_protocol_(*this),
      protocol_phase_(ProtocolPhases::SENDING_INIT_1),
      display_status_(true),
      force_send_control_(false),
      forced_publish_(false),
      forced_request_status_(false),
      control_called_(false) {
  this->traits_ = climate::ClimateTraits();
  this->traits_.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT,
                                     climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_MODE_DRY,
                                     climate::CLIMATE_MODE_AUTO});
  this->traits_.set_supported_fan_modes(
      {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH});
  this->traits_.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH,
                                           climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL});
  this->traits_.set_supports_current_temperature(true);
}

HaierClimateBase::~HaierClimateBase() {}

void HaierClimateBase::set_phase_(ProtocolPhases phase) {
  if (this->protocol_phase_ != phase) {
#if (HAIER_LOG_LEVEL > 4)
    ESP_LOGV(TAG, "Phase transition: %s => %s", phase_to_string_(this->protocol_phase_), phase_to_string_(phase));
#else
    ESP_LOGV(TAG, "Phase transition: %d => %d", this->protocol_phase_, phase);
#endif
    this->protocol_phase_ = phase;
  }
}

bool HaierClimateBase::check_timout_(std::chrono::steady_clock::time_point now,
                                     std::chrono::steady_clock::time_point tpoint, size_t timeout) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - tpoint).count() > timeout;
}

bool HaierClimateBase::is_message_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return this->check_timout_(now, this->last_request_timestamp_, DEFAULT_MESSAGES_INTERVAL_MS);
}

bool HaierClimateBase::is_status_request_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return this->check_timout_(now, this->last_status_request_, STATUS_REQUEST_INTERVAL_MS);
}

bool HaierClimateBase::is_control_message_timeout_exceeded_(std::chrono::steady_clock::time_point now) {
  return this->check_timout_(now, this->control_request_timestamp_, CONTROL_TIMEOUT_MS);
}

bool HaierClimateBase::is_control_message_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return this->check_timout_(now, this->last_request_timestamp_, CONTROL_MESSAGES_INTERVAL_MS);
}

bool HaierClimateBase::is_protocol_initialisation_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return this->check_timout_(now, this->last_request_timestamp_, PROTOCOL_INITIALIZATION_INTERVAL);
}

bool HaierClimateBase::get_display_state() const { return this->display_status_; }

void HaierClimateBase::set_display_state(bool state) {
  if (this->display_status_ != state) {
    this->display_status_ = state;
    this->force_send_control_ = true;
  }
}

void HaierClimateBase::set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes) {
  this->traits_.set_supported_swing_modes(modes);
  this->traits_.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);       // Always available
  this->traits_.add_supported_swing_mode(climate::CLIMATE_SWING_VERTICAL);  // Always available
}

void HaierClimateBase::set_supported_modes(const std::set<climate::ClimateMode> &modes) {
  this->traits_.set_supported_modes(modes);
  this->traits_.add_supported_mode(climate::CLIMATE_MODE_OFF);   // Always available
  this->traits_.add_supported_mode(climate::CLIMATE_MODE_AUTO);  // Always available
}

haier_protocol::HandlerError HaierClimateBase::answer_preprocess_(uint8_t request_message_type,
                                                                  uint8_t expected_request_message_type,
                                                                  uint8_t answer_message_type,
                                                                  uint8_t expected_answer_message_type,
                                                                  ProtocolPhases expected_phase) {
  haier_protocol::HandlerError result = haier_protocol::HandlerError::HANDLER_OK;
  if ((expected_request_message_type != NO_COMMAND) && (request_message_type != expected_request_message_type))
    result = haier_protocol::HandlerError::UNSUPORTED_MESSAGE;
  if ((expected_answer_message_type != NO_COMMAND) && (answer_message_type != expected_answer_message_type))
    result = haier_protocol::HandlerError::UNSUPORTED_MESSAGE;
  if ((expected_phase != ProtocolPhases::UNKNOWN) && (expected_phase != this->protocol_phase_))
    result = haier_protocol::HandlerError::UNEXPECTED_MESSAGE;
  if (is_message_invalid(answer_message_type))
    result = haier_protocol::HandlerError::INVALID_ANSWER;
  return result;
}

haier_protocol::HandlerError HaierClimateBase::timeout_default_handler_(uint8_t request_type) {
#if (HAIER_LOG_LEVEL > 4)
  ESP_LOGW(TAG, "Answer timeout for command %02X, phase %s", request_type, phase_to_string_(this->protocol_phase_));
#else
  ESP_LOGW(TAG, "Answer timeout for command %02X, phase %d", request_type, this->protocol_phase_);
#endif
  if (this->protocol_phase_ > ProtocolPhases::IDLE) {
    this->set_phase_(ProtocolPhases::IDLE);
  } else {
    this->set_phase_(ProtocolPhases::SENDING_INIT_1);
  }
  return haier_protocol::HandlerError::HANDLER_OK;
}

void HaierClimateBase::setup() {
  ESP_LOGI(TAG, "Haier initialization...");
  // Set timestamp here to give AC time to boot
  this->last_request_timestamp_ = std::chrono::steady_clock::now();
  this->set_phase_(ProtocolPhases::SENDING_INIT_1);
  this->set_answers_handlers();
  this->haier_protocol_.set_default_timeout_handler(
      std::bind(&esphome::haier::HaierClimateBase::timeout_default_handler_, this, std::placeholders::_1));
}

void HaierClimateBase::dump_config() {
  LOG_CLIMATE("", "Haier Climate", this);
  ESP_LOGCONFIG(TAG, "  Device communication status: %s",
                (this->protocol_phase_ >= ProtocolPhases::IDLE) ? "established" : "none");
}

void HaierClimateBase::loop() {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_valid_status_timestamp_).count() >
      COMMUNICATION_TIMEOUT_MS) {
    if (this->protocol_phase_ >= ProtocolPhases::IDLE) {
      // No status too long, reseting protocol
      ESP_LOGW(TAG, "Communication timeout, reseting protocol");
      this->last_valid_status_timestamp_ = now;
      this->force_send_control_ = false;
      if (this->hvac_settings_.valid)
        this->hvac_settings_.reset();
      this->set_phase_(ProtocolPhases::SENDING_INIT_1);
      return;
    } else {
      // No need to reset protocol if we didn't pass initialization phase
      this->last_valid_status_timestamp_ = now;
    }
  };
  if (this->hvac_settings_.valid || this->force_send_control_) {
    // If control message is pending we should send it ASAP unless we are in initialisation procedure or waiting for an
    // answer
    if ((this->protocol_phase_ == ProtocolPhases::IDLE) ||
        (this->protocol_phase_ == ProtocolPhases::SENDING_STATUS_REQUEST) ||
        (this->protocol_phase_ == ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST) ||
        (this->protocol_phase_ == ProtocolPhases::SENDING_SIGNAL_LEVEL)) {
      ESP_LOGV(TAG, "Control packet is pending...");
      this->control_request_timestamp_ = now;
      this->set_phase_(ProtocolPhases::SENDING_CONTROL);
    }
  }
  this->process_phase(now);
  this->haier_protocol_.loop();
}

ClimateTraits HaierClimateBase::traits() { return traits_; }

void HaierClimateBase::control(const ClimateCall &call) {
  ESP_LOGD("Control", "Control call");
  if (this->protocol_phase_ < ProtocolPhases::IDLE) {
    ESP_LOGW(TAG, "Can't send control packet, first poll answer not received");
    return;  // cancel the control, we cant do it without a poll answer.
  }
  if (this->hvac_settings_.valid) {
    ESP_LOGW(TAG, "Overriding old valid settings before they were applied!");
  }
  {
    if (call.get_mode().has_value())
      this->hvac_settings_.mode = call.get_mode();
    if (call.get_fan_mode().has_value())
      this->hvac_settings_.fan_mode = call.get_fan_mode();
    if (call.get_swing_mode().has_value())
      this->hvac_settings_.swing_mode = call.get_swing_mode();
    if (call.get_target_temperature().has_value())
      this->hvac_settings_.target_temperature = call.get_target_temperature();
    if (call.get_preset().has_value())
      this->hvac_settings_.preset = call.get_preset();
    this->hvac_settings_.valid = true;
  }
  this->control_called_ = true;
}

void HaierClimateBase::HvacSettings::reset() {
  this->valid = false;
  this->mode.reset();
  this->fan_mode.reset();
  this->swing_mode.reset();
  this->target_temperature.reset();
  this->preset.reset();
}

void HaierClimateBase::send_message_(const haier_protocol::HaierMessage &command, bool use_crc) {
  this->haier_protocol_.send_message(command, use_crc);
  this->last_request_timestamp_ = std::chrono::steady_clock::now();
}

}  // namespace haier
}  // namespace esphome
