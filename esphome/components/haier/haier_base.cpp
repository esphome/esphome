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

const char *HaierClimateBase::phase_to_string_(ProtocolPhases phase) {
  static const char *phase_names[] = {
      "SENDING_INIT_1",
      "SENDING_INIT_2",
      "SENDING_FIRST_STATUS_REQUEST",
      "SENDING_FIRST_ALARM_STATUS_REQUEST",
      "IDLE",
      "SENDING_STATUS_REQUEST",
      "SENDING_UPDATE_SIGNAL_REQUEST",
      "SENDING_SIGNAL_LEVEL",
      "SENDING_CONTROL",
      "SENDING_ACTION_COMMAND",
      "SENDING_ALARM_STATUS_REQUEST",
      "UNKNOWN"  // Should be the last!
  };
  static_assert(
      (sizeof(phase_names) / sizeof(char *)) == (((int) ProtocolPhases::NUM_PROTOCOL_PHASES) + 1),
      "Wrong phase_names array size. Please, make sure that this array is aligned with the enum ProtocolPhases");
  int phase_index = (int) phase;
  if ((phase_index > (int) ProtocolPhases::NUM_PROTOCOL_PHASES) || (phase_index < 0))
    phase_index = (int) ProtocolPhases::NUM_PROTOCOL_PHASES;
  return phase_names[phase_index];
}

bool check_timeout(std::chrono::steady_clock::time_point now, std::chrono::steady_clock::time_point tpoint,
                   size_t timeout) {
  return std::chrono::duration_cast<std::chrono::milliseconds>(now - tpoint).count() > timeout;
}

HaierClimateBase::HaierClimateBase()
    : haier_protocol_(*this),
      protocol_phase_(ProtocolPhases::SENDING_INIT_1),
      force_send_control_(false),
      forced_request_status_(false),
      reset_protocol_request_(false),
      send_wifi_signal_(true),
      use_crc_(false) {
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
    ESP_LOGV(TAG, "Phase transition: %s => %s", phase_to_string_(this->protocol_phase_), phase_to_string_(phase));
    this->protocol_phase_ = phase;
  }
}

void HaierClimateBase::reset_phase_() {
  this->set_phase((this->protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE
                                                                  : ProtocolPhases::SENDING_INIT_1);
}

void HaierClimateBase::reset_to_idle_() {
  this->force_send_control_ = false;
  if (this->current_hvac_settings_.valid)
    this->current_hvac_settings_.reset();
  this->forced_request_status_ = true;
  this->set_phase(ProtocolPhases::IDLE);
  this->action_request_.reset();
}

bool HaierClimateBase::is_message_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return check_timeout(now, this->last_request_timestamp_, DEFAULT_MESSAGES_INTERVAL_MS);
}

bool HaierClimateBase::is_status_request_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return check_timeout(now, this->last_status_request_, STATUS_REQUEST_INTERVAL_MS);
}

bool HaierClimateBase::is_control_message_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return check_timeout(now, this->last_request_timestamp_, CONTROL_MESSAGES_INTERVAL_MS);
}

bool HaierClimateBase::is_protocol_initialisation_interval_exceeded_(std::chrono::steady_clock::time_point now) {
  return check_timeout(now, this->last_request_timestamp_, PROTOCOL_INITIALIZATION_INTERVAL);
}

#ifdef USE_WIFI
haier_protocol::HaierMessage HaierClimateBase::get_wifi_signal_message_() {
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
  return haier_protocol::HaierMessage(haier_protocol::FrameType::REPORT_NETWORK_STATUS, wifi_status_data,
                                      sizeof(wifi_status_data));
}
#endif

void HaierClimateBase::save_settings() {
  HaierBaseSettings settings{this->get_health_mode(), this->get_display_state()};
  if (!this->base_rtc_.save(&settings)) {
    ESP_LOGW(TAG, "Failed to save settings");
  }
}

bool HaierClimateBase::get_display_state() const {
  return (this->display_status_ == SwitchState::ON) || (this->display_status_ == SwitchState::PENDING_ON);
}

void HaierClimateBase::set_display_state(bool state) {
  if (state != this->get_display_state()) {
    this->display_status_ = state ? SwitchState::PENDING_ON : SwitchState::PENDING_OFF;
    this->force_send_control_ = true;
    this->save_settings();
  }
}

bool HaierClimateBase::get_health_mode() const {
  return (this->health_mode_ == SwitchState::ON) || (this->health_mode_ == SwitchState::PENDING_ON);
}

void HaierClimateBase::set_health_mode(bool state) {
  if (state != this->get_health_mode()) {
    this->health_mode_ = state ? SwitchState::PENDING_ON : SwitchState::PENDING_OFF;
    this->force_send_control_ = true;
    this->save_settings();
  }
}

void HaierClimateBase::send_power_on_command() {
  this->action_request_ =
      PendingAction({ActionRequest::TURN_POWER_ON, esphome::optional<haier_protocol::HaierMessage>()});
}

void HaierClimateBase::send_power_off_command() {
  this->action_request_ =
      PendingAction({ActionRequest::TURN_POWER_OFF, esphome::optional<haier_protocol::HaierMessage>()});
}

void HaierClimateBase::toggle_power() {
  this->action_request_ =
      PendingAction({ActionRequest::TOGGLE_POWER, esphome::optional<haier_protocol::HaierMessage>()});
}

void HaierClimateBase::set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes) {
  this->traits_.set_supported_swing_modes(modes);
  if (!modes.empty())
    this->traits_.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);
}

void HaierClimateBase::set_answer_timeout(uint32_t timeout) { this->haier_protocol_.set_answer_timeout(timeout); }

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

void HaierClimateBase::send_custom_command(const haier_protocol::HaierMessage &message) {
  this->action_request_ = PendingAction({ActionRequest::SEND_CUSTOM_COMMAND, message});
}

void HaierClimateBase::add_status_message_callback(std::function<void(const char *, size_t)> &&callback) {
  this->status_message_callback_.add(std::move(callback));
}

haier_protocol::HandlerError HaierClimateBase::answer_preprocess_(
    haier_protocol::FrameType request_message_type, haier_protocol::FrameType expected_request_message_type,
    haier_protocol::FrameType answer_message_type, haier_protocol::FrameType expected_answer_message_type,
    ProtocolPhases expected_phase) {
  haier_protocol::HandlerError result = haier_protocol::HandlerError::HANDLER_OK;
  if ((expected_request_message_type != haier_protocol::FrameType::UNKNOWN_FRAME_TYPE) &&
      (request_message_type != expected_request_message_type))
    result = haier_protocol::HandlerError::UNSUPPORTED_MESSAGE;
  if ((expected_answer_message_type != haier_protocol::FrameType::UNKNOWN_FRAME_TYPE) &&
      (answer_message_type != expected_answer_message_type))
    result = haier_protocol::HandlerError::UNSUPPORTED_MESSAGE;
  if (!this->haier_protocol_.is_waiting_for_answer() ||
      ((expected_phase != ProtocolPhases::UNKNOWN) && (expected_phase != this->protocol_phase_)))
    result = haier_protocol::HandlerError::UNEXPECTED_MESSAGE;
  if (answer_message_type == haier_protocol::FrameType::INVALID)
    result = haier_protocol::HandlerError::INVALID_ANSWER;
  return result;
}

haier_protocol::HandlerError HaierClimateBase::report_network_status_answer_handler_(
    haier_protocol::FrameType request_type, haier_protocol::FrameType message_type, const uint8_t *data,
    size_t data_size) {
  haier_protocol::HandlerError result =
      this->answer_preprocess_(request_type, haier_protocol::FrameType::REPORT_NETWORK_STATUS, message_type,
                               haier_protocol::FrameType::CONFIRM, ProtocolPhases::SENDING_SIGNAL_LEVEL);
  this->set_phase(ProtocolPhases::IDLE);
  return result;
}

haier_protocol::HandlerError HaierClimateBase::timeout_default_handler_(haier_protocol::FrameType request_type) {
  ESP_LOGW(TAG, "Answer timeout for command %02X, phase %s", (uint8_t) request_type,
           phase_to_string_(this->protocol_phase_));
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
  this->haier_protocol_.set_default_timeout_handler(
      std::bind(&esphome::haier::HaierClimateBase::timeout_default_handler_, this, std::placeholders::_1));
  this->set_handlers();
  this->initialization();
}

void HaierClimateBase::dump_config() {
  LOG_CLIMATE("", "Haier Climate", this);
  ESP_LOGCONFIG(TAG, "  Device communication status: %s", this->valid_connection() ? "established" : "none");
}

void HaierClimateBase::loop() {
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  if ((std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_valid_status_timestamp_).count() >
       COMMUNICATION_TIMEOUT_MS) ||
      (this->reset_protocol_request_ && (!this->haier_protocol_.is_waiting_for_answer()))) {
    this->last_valid_status_timestamp_ = now;
    if (this->protocol_phase_ >= ProtocolPhases::IDLE) {
      // No status too long, reseting protocol
      // No need to reset protocol if we didn't pass initialization phase
      if (this->reset_protocol_request_) {
        this->reset_protocol_request_ = false;
        ESP_LOGW(TAG, "Protocol reset requested");
      } else {
        ESP_LOGW(TAG, "Communication timeout, reseting protocol");
      }
      this->process_protocol_reset();
      return;
    }
  };
  if ((!this->haier_protocol_.is_waiting_for_answer()) &&
      ((this->protocol_phase_ == ProtocolPhases::IDLE) ||
       (this->protocol_phase_ == ProtocolPhases::SENDING_STATUS_REQUEST) ||
       (this->protocol_phase_ == ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST) ||
       (this->protocol_phase_ == ProtocolPhases::SENDING_SIGNAL_LEVEL))) {
    // If control message or action is pending we should send it ASAP unless we are in initialisation
    // procedure or waiting for an answer
    if (this->action_request_.has_value() && this->prepare_pending_action()) {
      this->set_phase(ProtocolPhases::SENDING_ACTION_COMMAND);
    } else if (this->next_hvac_settings_.valid || this->force_send_control_) {
      ESP_LOGV(TAG, "Control packet is pending...");
      this->set_phase(ProtocolPhases::SENDING_CONTROL);
      if (this->next_hvac_settings_.valid) {
        this->current_hvac_settings_ = this->next_hvac_settings_;
        this->next_hvac_settings_.reset();
      } else {
        this->current_hvac_settings_.reset();
      }
    }
  }
  this->process_phase(now);
  this->haier_protocol_.loop();
#ifdef USE_SWITCH
  if ((this->display_switch_ != nullptr) && (this->display_switch_->state != this->get_display_state())) {
    this->display_switch_->publish_state(this->get_display_state());
  }
  if ((this->health_mode_switch_ != nullptr) && (this->health_mode_switch_->state != this->get_health_mode())) {
    this->health_mode_switch_->publish_state(this->get_health_mode());
  }
#endif  // USE_SWITCH
}

void HaierClimateBase::process_protocol_reset() {
  this->force_send_control_ = false;
  if (this->current_hvac_settings_.valid)
    this->current_hvac_settings_.reset();
  if (this->next_hvac_settings_.valid)
    this->next_hvac_settings_.reset();
  this->mode = CLIMATE_MODE_OFF;
  this->current_temperature = NAN;
  this->target_temperature = NAN;
  this->fan_mode.reset();
  this->preset.reset();
  this->publish_state();
  this->set_phase(ProtocolPhases::SENDING_INIT_1);
}

bool HaierClimateBase::prepare_pending_action() {
  if (this->action_request_.has_value()) {
    switch (this->action_request_.value().action) {
      case ActionRequest::SEND_CUSTOM_COMMAND:
        return true;
      case ActionRequest::TURN_POWER_ON:
        this->action_request_.value().message = this->get_power_message(true);
        return true;
      case ActionRequest::TURN_POWER_OFF:
        this->action_request_.value().message = this->get_power_message(false);
        return true;
      case ActionRequest::TOGGLE_POWER:
        this->action_request_.value().message = this->get_power_message(this->mode == ClimateMode::CLIMATE_MODE_OFF);
        return true;
      default:
        ESP_LOGW(TAG, "Unsupported action: %d", (uint8_t) this->action_request_.value().action);
        this->action_request_.reset();
        return false;
    }
  } else {
    return false;
  }
}

ClimateTraits HaierClimateBase::traits() { return traits_; }

void HaierClimateBase::initialization() {
  constexpr uint32_t restore_settings_version = 0xA77D21EF;
  this->base_rtc_ =
      global_preferences->make_preference<HaierBaseSettings>(this->get_object_id_hash() ^ restore_settings_version);
  HaierBaseSettings recovered;
  if (!this->base_rtc_.load(&recovered)) {
    recovered = {false, true};
  }
  this->display_status_ = recovered.display_state ? SwitchState::PENDING_ON : SwitchState::PENDING_OFF;
  this->health_mode_ = recovered.health_mode ? SwitchState::PENDING_ON : SwitchState::PENDING_OFF;
#ifdef USE_SWITCH
  if (this->display_switch_ != nullptr) {
    this->display_switch_->publish_state(this->get_display_state());
  }
  if (this->health_mode_switch_ != nullptr) {
    this->health_mode_switch_->publish_state(this->get_health_mode());
  }
#endif
}

void HaierClimateBase::control(const ClimateCall &call) {
  ESP_LOGD("Control", "Control call");
  if (!this->valid_connection()) {
    ESP_LOGW(TAG, "Can't send control packet, first poll answer not received");
    return;  // cancel the control, we cant do it without a poll answer.
  }
  if (this->current_hvac_settings_.valid) {
    ESP_LOGW(TAG, "New settings come faster then processed!");
  }
  {
    if (call.get_mode().has_value())
      this->next_hvac_settings_.mode = call.get_mode();
    if (call.get_fan_mode().has_value())
      this->next_hvac_settings_.fan_mode = call.get_fan_mode();
    if (call.get_swing_mode().has_value())
      this->next_hvac_settings_.swing_mode = call.get_swing_mode();
    if (call.get_target_temperature().has_value())
      this->next_hvac_settings_.target_temperature = call.get_target_temperature();
    if (call.get_preset().has_value())
      this->next_hvac_settings_.preset = call.get_preset();
    this->next_hvac_settings_.valid = true;
  }
}

#ifdef USE_SWITCH
void HaierClimateBase::set_display_switch(switch_::Switch *sw) {
  this->display_switch_ = sw;
  if ((this->display_switch_ != nullptr) && (this->valid_connection())) {
    this->display_switch_->publish_state(this->get_display_state());
  }
}

void HaierClimateBase::set_health_mode_switch(switch_::Switch *sw) {
  this->health_mode_switch_ = sw;
  if ((this->health_mode_switch_ != nullptr) && (this->valid_connection())) {
    this->health_mode_switch_->publish_state(this->get_health_mode());
  }
}
#endif

void HaierClimateBase::HvacSettings::reset() {
  this->valid = false;
  this->mode.reset();
  this->fan_mode.reset();
  this->swing_mode.reset();
  this->target_temperature.reset();
  this->preset.reset();
}

void HaierClimateBase::send_message_(const haier_protocol::HaierMessage &command, bool use_crc, uint8_t num_repeats,
                                     std::chrono::milliseconds interval) {
  this->haier_protocol_.send_message(command, use_crc, num_repeats, interval);
  this->last_request_timestamp_ = std::chrono::steady_clock::now();
}

}  // namespace haier
}  // namespace esphome
