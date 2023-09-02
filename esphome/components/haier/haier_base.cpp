#include <chrono>
#include <string>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#ifdef USE_WIFI
#include "esphome/components/wifi/wifi_component.h"
#endif
#include "haier_base.h"

using namespace esphome::climate;
using namespace esphome::uart;

namespace esphome {
namespace haier {

static const char *const TAG = "haier.climate";
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
      "WAITING_INIT_1_ANSWER",
      "SENDING_INIT_2",
      "WAITING_INIT_2_ANSWER",
      "SENDING_FIRST_STATUS_REQUEST",
      "WAITING_FIRST_STATUS_ANSWER",
      "SENDING_ALARM_STATUS_REQUEST",
      "WAITING_ALARM_STATUS_ANSWER",
      "IDLE",
      "UNKNOWN",
      "SENDING_STATUS_REQUEST",
      "WAITING_STATUS_ANSWER",
      "SENDING_UPDATE_SIGNAL_REQUEST",
      "WAITING_UPDATE_SIGNAL_ANSWER",
      "SENDING_SIGNAL_LEVEL",
      "WAITING_SIGNAL_LEVEL_ANSWER",
      "SENDING_CONTROL",
      "WAITING_CONTROL_ANSWER",
      "SENDING_POWER_ON_COMMAND",
      "WAITING_POWER_ON_ANSWER",
      "SENDING_POWER_OFF_COMMAND",
      "WAITING_POWER_OFF_ANSWER",
      "UNKNOWN"  // Should be the last!
  };
  int phase_index = (int) phase;
  if ((phase_index > (int) ProtocolPhases::NUM_PROTOCOL_PHASES) || (phase_index < 0))
    phase_index = (int) ProtocolPhases::NUM_PROTOCOL_PHASES;
  return phase_names[phase_index];
}
#endif

HaierClimateBase::HaierClimateBase()
    : haier_protocol_(*this),
      protocol_phase_(ProtocolPhases::SENDING_INIT_1),
      action_request_(ActionRequest::NO_ACTION),
      display_status_(true),
      health_mode_(false),
      force_send_control_(false),
      forced_publish_(false),
      forced_request_status_(false),
      first_control_attempt_(false),
      reset_protocol_request_(false),
      send_wifi_signal_(true) {
  this->traits_ = climate::ClimateTraits();
  this->traits_.set_supported_modes({climate::CLIMATE_MODE_OFF, climate::CLIMATE_MODE_COOL, climate::CLIMATE_MODE_HEAT,
                                     climate::CLIMATE_MODE_FAN_ONLY, climate::CLIMATE_MODE_DRY,
                                     climate::CLIMATE_MODE_HEAT_COOL});
  this->traits_.set_supported_fan_modes(
      {climate::CLIMATE_FAN_AUTO, climate::CLIMATE_FAN_LOW, climate::CLIMATE_FAN_MEDIUM, climate::CLIMATE_FAN_HIGH});
  this->traits_.set_supported_swing_modes({climate::CLIMATE_SWING_OFF, climate::CLIMATE_SWING_BOTH,
                                           climate::CLIMATE_SWING_VERTICAL, climate::CLIMATE_SWING_HORIZONTAL});
  this->traits_.set_supports_current_temperature(true);
}

HaierClimateBase::~HaierClimateBase() {}

void HaierClimateBase::set_phase(ProtocolPhases phase) {
  if (this->protocol_phase_ != phase) {
#if (HAIER_LOG_LEVEL > 4)
    ESP_LOGV(TAG, "Phase transition: %s => %s", phase_to_string_(this->protocol_phase_), phase_to_string_(phase));
#else
    ESP_LOGV(TAG, "Phase transition: %d => %d", (int) this->protocol_phase_, (int) phase);
#endif
    this->protocol_phase_ = phase;
  }
}

bool HaierClimateBase::check_timeout_(std::chrono::steady_clock::time_point now,
                                      std::chrono::steady_clock::time_point tpoint, size_t timeout) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - tpoint).count() > timeout;
}

bool HaierClimateBase::is_message_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return this->check_timeout_(now, this->last_request_timestamp_, DEFAULT_MESSAGES_INTERVAL_MS);
}

bool HaierClimateBase::is_status_request_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return this->check_timeout_(now, this->last_status_request_, STATUS_REQUEST_INTERVAL_MS);
}

bool HaierClimateBase::is_control_message_timeout_exceeded_(std::chrono::steady_clock::time_point now) {
  return this->check_timeout_(now, this->control_request_timestamp_, CONTROL_TIMEOUT_MS);
}

bool HaierClimateBase::is_control_message_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return this->check_timeout_(now, this->last_request_timestamp_, CONTROL_MESSAGES_INTERVAL_MS);
}

bool HaierClimateBase::is_protocol_initialisation_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return this->check_timeout_(now, this->last_request_timestamp_, PROTOCOL_INITIALIZATION_INTERVAL);
}

#ifdef USE_WIFI
haier_protocol::HaierMessage HaierClimateBase::get_wifi_signal_message_(uint8_t message_type) {
  static uint8_t wifi_status_data[4] = {0x00, 0x00, 0x00, 0x00};
  if (wifi::global_wifi_component->is_connected()) {
    wifi_status_data[1] = 0;
    int8_t rssi = wifi::global_wifi_component->wifi_rssi();
    wifi_status_data[3] = uint8_t((128 + rssi) / 1.28f);
    ESP_LOGD(TAG, "WiFi signal is: %ddBm => %d%%", rssi, wifi_status_data[3]);
  } else {
    ESP_LOGD(TAG, "WiFi is not connected");
    wifi_status_data[1] = 1;
    wifi_status_data[3] = 0;
  }
  return haier_protocol::HaierMessage(message_type, wifi_status_data, sizeof(wifi_status_data));
}
#endif

bool HaierClimateBase::get_display_state() const { return this->display_status_; }

void HaierClimateBase::set_display_state(bool state) {
  if (this->display_status_ != state) {
    this->display_status_ = state;
    this->set_force_send_control_(true);
  }
}

bool HaierClimateBase::get_health_mode() const { return this->health_mode_; }

void HaierClimateBase::set_health_mode(bool state) {
  if (this->health_mode_ != state) {
    this->health_mode_ = state;
    this->set_force_send_control_(true);
  }
}

void HaierClimateBase::send_power_on_command() { this->action_request_ = ActionRequest::TURN_POWER_ON; }

void HaierClimateBase::send_power_off_command() { this->action_request_ = ActionRequest::TURN_POWER_OFF; }

void HaierClimateBase::toggle_power() { this->action_request_ = ActionRequest::TOGGLE_POWER; }

void HaierClimateBase::set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes) {
  this->traits_.set_supported_swing_modes(modes);
  if (!modes.empty())
    this->traits_.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
}

void HaierClimateBase::set_answer_timeout(uint32_t timeout) {
  this->answer_timeout_ = std::chrono::milliseconds(timeout);
}

void HaierClimateBase::set_supported_modes(const std::set<climate::ClimateMode> &modes) {
  this->traits_.set_supported_modes(modes);
  this->traits_.add_supported_mode(climate::CLIMATE_MODE_OFF);        // Always available
  this->traits_.add_supported_mode(climate::CLIMATE_MODE_HEAT_COOL);  // Always available
}

void HaierClimateBase::set_supported_presets(const std::set<climate::ClimatePreset> &presets) {
  this->traits_.set_supported_presets(presets);
  if (!presets.empty())
    this->traits_.add_supported_preset(climate::CLIMATE_PRESET_NONE);
}

void HaierClimateBase::set_send_wifi(bool send_wifi) { this->send_wifi_signal_ = send_wifi; }

haier_protocol::HandlerError HaierClimateBase::answer_preprocess_(uint8_t request_message_type,
                                                                  uint8_t expected_request_message_type,
                                                                  uint8_t answer_message_type,
                                                                  uint8_t expected_answer_message_type,
                                                                  ProtocolPhases expected_phase) {
  haier_protocol::HandlerError result = haier_protocol::HandlerError::HANDLER_OK;
  if ((expected_request_message_type != NO_COMMAND) && (request_message_type != expected_request_message_type))
    result = haier_protocol::HandlerError::UNSUPPORTED_MESSAGE;
  if ((expected_answer_message_type != NO_COMMAND) && (answer_message_type != expected_answer_message_type))
    result = haier_protocol::HandlerError::UNSUPPORTED_MESSAGE;
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
  ESP_LOGW(TAG, "Answer timeout for command %02X, phase %d", request_type, (int) this->protocol_phase_);
#endif
  if (this->protocol_phase_ > ProtocolPhases::IDLE) {
    this->set_phase(ProtocolPhases::IDLE);
  } else {
    this->set_phase(ProtocolPhases::SENDING_INIT_1);
  }
  return haier_protocol::HandlerError::HANDLER_OK;
}

void HaierClimateBase::setup() {
  ESP_LOGI(TAG, "Haier initialization...");
  // Set timestamp here to give AC time to boot
  this->last_request_timestamp_ = std::chrono::steady_clock::now();
  this->set_phase(ProtocolPhases::SENDING_INIT_1);
  this->set_handlers();
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
  if ((std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_valid_status_timestamp_).count() >
       COMMUNICATION_TIMEOUT_MS) ||
      (this->reset_protocol_request_)) {
    if (this->protocol_phase_ >= ProtocolPhases::IDLE) {
      // No status too long, reseting protocol
      if (this->reset_protocol_request_) {
        this->reset_protocol_request_ = false;
        ESP_LOGW(TAG, "Protocol reset requested");
      } else {
        ESP_LOGW(TAG, "Communication timeout, reseting protocol");
      }
      this->last_valid_status_timestamp_ = now;
      this->set_force_send_control_(false);
      if (this->hvac_settings_.valid)
        this->hvac_settings_.reset();
      this->set_phase(ProtocolPhases::SENDING_INIT_1);
      return;
    } else {
      // No need to reset protocol if we didn't pass initialization phase
      this->last_valid_status_timestamp_ = now;
    }
  };
  if ((this->protocol_phase_ == ProtocolPhases::IDLE) ||
      (this->protocol_phase_ == ProtocolPhases::SENDING_STATUS_REQUEST) ||
      (this->protocol_phase_ == ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST) ||
      (this->protocol_phase_ == ProtocolPhases::SENDING_SIGNAL_LEVEL)) {
    // If control message or action is pending we should send it ASAP unless we are in initialisation
    // procedure or waiting for an answer
    if (this->action_request_ != ActionRequest::NO_ACTION) {
      this->process_pending_action();
    } else if (this->hvac_settings_.valid || this->force_send_control_) {
      ESP_LOGV(TAG, "Control packet is pending...");
      this->set_phase(ProtocolPhases::SENDING_CONTROL);
    }
  }
  this->process_phase(now);
  this->haier_protocol_.loop();
}

void HaierClimateBase::process_pending_action() {
  ActionRequest request = this->action_request_;
  if (this->action_request_ == ActionRequest::TOGGLE_POWER) {
    request = this->mode == CLIMATE_MODE_OFF ? ActionRequest::TURN_POWER_ON : ActionRequest::TURN_POWER_OFF;
  }
  switch (request) {
    case ActionRequest::TURN_POWER_ON:
      this->set_phase(ProtocolPhases::SENDING_POWER_ON_COMMAND);
      break;
    case ActionRequest::TURN_POWER_OFF:
      this->set_phase(ProtocolPhases::SENDING_POWER_OFF_COMMAND);
      break;
    case ActionRequest::TOGGLE_POWER:
    case ActionRequest::NO_ACTION:
      // shouldn't get here, do nothing
      break;
    default:
      ESP_LOGW(TAG, "Unsupported action: %d", (uint8_t) this->action_request_);
      break;
  }
  this->action_request_ = ActionRequest::NO_ACTION;
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
  this->first_control_attempt_ = true;
}

void HaierClimateBase::HvacSettings::reset() {
  this->valid = false;
  this->mode.reset();
  this->fan_mode.reset();
  this->swing_mode.reset();
  this->target_temperature.reset();
  this->preset.reset();
}

void HaierClimateBase::set_force_send_control_(bool status) {
  this->force_send_control_ = status;
  if (status) {
    this->first_control_attempt_ = true;
  }
}

void HaierClimateBase::send_message_(const haier_protocol::HaierMessage &command, bool use_crc) {
  if (this->answer_timeout_.has_value()) {
    this->haier_protocol_.send_message(command, use_crc, this->answer_timeout_.value());
  } else {
    this->haier_protocol_.send_message(command, use_crc);
  }
  this->last_request_timestamp_ = std::chrono::steady_clock::now();
}

}  // namespace haier
}  // namespace esphome
