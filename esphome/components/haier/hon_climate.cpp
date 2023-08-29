#include <chrono>
#include <string>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "hon_climate.h"
#include "hon_packet.h"

using namespace esphome::climate;
using namespace esphome::uart;

namespace esphome {
namespace haier {

static const char *const TAG = "haier.climate";
constexpr size_t SIGNAL_LEVEL_UPDATE_INTERVAL_MS = 10000;
constexpr int PROTOCOL_OUTDOOR_TEMPERATURE_OFFSET = -64;

hon_protocol::VerticalSwingMode get_vertical_swing_mode(AirflowVerticalDirection direction) {
  switch (direction) {
    case AirflowVerticalDirection::HEALTH_UP:
      return hon_protocol::VerticalSwingMode::HEALTH_UP;
    case AirflowVerticalDirection::MAX_UP:
      return hon_protocol::VerticalSwingMode::MAX_UP;
    case AirflowVerticalDirection::UP:
      return hon_protocol::VerticalSwingMode::UP;
    case AirflowVerticalDirection::DOWN:
      return hon_protocol::VerticalSwingMode::DOWN;
    case AirflowVerticalDirection::HEALTH_DOWN:
      return hon_protocol::VerticalSwingMode::HEALTH_DOWN;
    default:
      return hon_protocol::VerticalSwingMode::CENTER;
  }
}

hon_protocol::HorizontalSwingMode get_horizontal_swing_mode(AirflowHorizontalDirection direction) {
  switch (direction) {
    case AirflowHorizontalDirection::MAX_LEFT:
      return hon_protocol::HorizontalSwingMode::MAX_LEFT;
    case AirflowHorizontalDirection::LEFT:
      return hon_protocol::HorizontalSwingMode::LEFT;
    case AirflowHorizontalDirection::RIGHT:
      return hon_protocol::HorizontalSwingMode::RIGHT;
    case AirflowHorizontalDirection::MAX_RIGHT:
      return hon_protocol::HorizontalSwingMode::MAX_RIGHT;
    default:
      return hon_protocol::HorizontalSwingMode::CENTER;
  }
}

HonClimate::HonClimate()
    : last_status_message_(new uint8_t[sizeof(hon_protocol::HaierPacketControl)]),
      cleaning_status_(CleaningState::NO_CLEANING),
      got_valid_outdoor_temp_(false),
      hvac_hardware_info_available_(false),
      hvac_functions_{false, false, false, false, false},
      use_crc_(hvac_functions_[2]),
      active_alarms_{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
      outdoor_sensor_(nullptr) {
  this->fan_mode_speed_ = (uint8_t) hon_protocol::FanMode::FAN_MID;
  this->other_modes_fan_speed_ = (uint8_t) hon_protocol::FanMode::FAN_AUTO;
}

HonClimate::~HonClimate() {}

void HonClimate::set_beeper_state(bool state) { this->beeper_status_ = state; }

bool HonClimate::get_beeper_state() const { return this->beeper_status_; }

void HonClimate::set_outdoor_temperature_sensor(esphome::sensor::Sensor *sensor) { this->outdoor_sensor_ = sensor; }

AirflowVerticalDirection HonClimate::get_vertical_airflow() const { return this->vertical_direction_; };

void HonClimate::set_vertical_airflow(AirflowVerticalDirection direction) {
  this->vertical_direction_ = direction;
  this->set_force_send_control_(true);
}

AirflowHorizontalDirection HonClimate::get_horizontal_airflow() const { return this->horizontal_direction_; }

void HonClimate::set_horizontal_airflow(AirflowHorizontalDirection direction) {
  this->horizontal_direction_ = direction;
  this->set_force_send_control_(true);
}

std::string HonClimate::get_cleaning_status_text() const {
  switch (this->cleaning_status_) {
    case CleaningState::SELF_CLEAN:
      return "Self clean";
    case CleaningState::STERI_CLEAN:
      return "56°C Steri-Clean";
    default:
      return "No cleaning";
  }
}

CleaningState HonClimate::get_cleaning_status() const { return this->cleaning_status_; }

void HonClimate::start_self_cleaning() {
  if (this->cleaning_status_ == CleaningState::NO_CLEANING) {
    ESP_LOGI(TAG, "Sending self cleaning start request");
    this->action_request_ = ActionRequest::START_SELF_CLEAN;
    this->set_force_send_control_(true);
  }
}

void HonClimate::start_steri_cleaning() {
  if (this->cleaning_status_ == CleaningState::NO_CLEANING) {
    ESP_LOGI(TAG, "Sending steri cleaning start request");
    this->action_request_ = ActionRequest::START_STERI_CLEAN;
    this->set_force_send_control_(true);
  }
}

haier_protocol::HandlerError HonClimate::get_device_version_answer_handler_(uint8_t request_type, uint8_t message_type,
                                                                            const uint8_t *data, size_t data_size) {
  // Should check this before preprocess
  if (message_type == (uint8_t) hon_protocol::FrameType::INVALID) {
    ESP_LOGW(TAG, "It looks like your ESPHome Haier climate configuration is wrong. You should use the smartAir2 "
                  "protocol instead of hOn");
    this->set_phase(ProtocolPhases::SENDING_INIT_1);
    return haier_protocol::HandlerError::INVALID_ANSWER;
  }
  haier_protocol::HandlerError result = this->answer_preprocess_(
      request_type, (uint8_t) hon_protocol::FrameType::GET_DEVICE_VERSION, message_type,
      (uint8_t) hon_protocol::FrameType::GET_DEVICE_VERSION_RESPONSE, ProtocolPhases::WAITING_INIT_1_ANSWER);
  if (result == haier_protocol::HandlerError::HANDLER_OK) {
    if (data_size < sizeof(hon_protocol::DeviceVersionAnswer)) {
      // Wrong structure
      this->set_phase(ProtocolPhases::SENDING_INIT_1);
      return haier_protocol::HandlerError::WRONG_MESSAGE_STRUCTURE;
    }
    // All OK
    hon_protocol::DeviceVersionAnswer *answr = (hon_protocol::DeviceVersionAnswer *) data;
    char tmp[9];
    tmp[8] = 0;
    strncpy(tmp, answr->protocol_version, 8);
    this->hvac_protocol_version_ = std::string(tmp);
    strncpy(tmp, answr->software_version, 8);
    this->hvac_software_version_ = std::string(tmp);
    strncpy(tmp, answr->hardware_version, 8);
    this->hvac_hardware_version_ = std::string(tmp);
    strncpy(tmp, answr->device_name, 8);
    this->hvac_device_name_ = std::string(tmp);
    this->hvac_functions_[0] = (answr->functions[1] & 0x01) != 0;  // interactive mode support
    this->hvac_functions_[1] = (answr->functions[1] & 0x02) != 0;  // controller-device mode support
    this->hvac_functions_[2] = (answr->functions[1] & 0x04) != 0;  // crc support
    this->hvac_functions_[3] = (answr->functions[1] & 0x08) != 0;  // multiple AC support
    this->hvac_functions_[4] = (answr->functions[1] & 0x20) != 0;  // roles support
    this->hvac_hardware_info_available_ = true;
    this->set_phase(ProtocolPhases::SENDING_INIT_2);
    return result;
  } else {
    this->set_phase((this->protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE
                                                                    : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

haier_protocol::HandlerError HonClimate::get_device_id_answer_handler_(uint8_t request_type, uint8_t message_type,
                                                                       const uint8_t *data, size_t data_size) {
  haier_protocol::HandlerError result = this->answer_preprocess_(
      request_type, (uint8_t) hon_protocol::FrameType::GET_DEVICE_ID, message_type,
      (uint8_t) hon_protocol::FrameType::GET_DEVICE_ID_RESPONSE, ProtocolPhases::WAITING_INIT_2_ANSWER);
  if (result == haier_protocol::HandlerError::HANDLER_OK) {
    this->set_phase(ProtocolPhases::SENDING_FIRST_STATUS_REQUEST);
    return result;
  } else {
    this->set_phase((this->protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE
                                                                    : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

haier_protocol::HandlerError HonClimate::status_handler_(uint8_t request_type, uint8_t message_type,
                                                         const uint8_t *data, size_t data_size) {
  haier_protocol::HandlerError result =
      this->answer_preprocess_(request_type, (uint8_t) hon_protocol::FrameType::CONTROL, message_type,
                               (uint8_t) hon_protocol::FrameType::STATUS, ProtocolPhases::UNKNOWN);
  if (result == haier_protocol::HandlerError::HANDLER_OK) {
    result = this->process_status_message_(data, data_size);
    if (result != haier_protocol::HandlerError::HANDLER_OK) {
      ESP_LOGW(TAG, "Error %d while parsing Status packet", (int) result);
      this->set_phase((this->protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE
                                                                      : ProtocolPhases::SENDING_INIT_1);
    } else {
      if (data_size >= sizeof(hon_protocol::HaierPacketControl) + 2) {
        memcpy(this->last_status_message_.get(), data + 2, sizeof(hon_protocol::HaierPacketControl));
      } else {
        ESP_LOGW(TAG, "Status packet too small: %d (should be >= %d)", data_size,
                 sizeof(hon_protocol::HaierPacketControl));
      }
      if (this->protocol_phase_ == ProtocolPhases::WAITING_FIRST_STATUS_ANSWER) {
        ESP_LOGI(TAG, "First HVAC status received");
        this->set_phase(ProtocolPhases::SENDING_ALARM_STATUS_REQUEST);
      } else if ((this->protocol_phase_ == ProtocolPhases::WAITING_STATUS_ANSWER) ||
                 (this->protocol_phase_ == ProtocolPhases::WAITING_POWER_ON_ANSWER) ||
                 (this->protocol_phase_ == ProtocolPhases::WAITING_POWER_OFF_ANSWER)) {
        this->set_phase(ProtocolPhases::IDLE);
      } else if (this->protocol_phase_ == ProtocolPhases::WAITING_CONTROL_ANSWER) {
        this->set_phase(ProtocolPhases::IDLE);
        this->set_force_send_control_(false);
        if (this->hvac_settings_.valid)
          this->hvac_settings_.reset();
      }
    }
    return result;
  } else {
    this->set_phase((this->protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE
                                                                    : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

haier_protocol::HandlerError HonClimate::get_management_information_answer_handler_(uint8_t request_type,
                                                                                    uint8_t message_type,
                                                                                    const uint8_t *data,
                                                                                    size_t data_size) {
  haier_protocol::HandlerError result =
      this->answer_preprocess_(request_type, (uint8_t) hon_protocol::FrameType::GET_MANAGEMENT_INFORMATION,
                               message_type, (uint8_t) hon_protocol::FrameType::GET_MANAGEMENT_INFORMATION_RESPONSE,
                               ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER);
  if (result == haier_protocol::HandlerError::HANDLER_OK) {
    this->set_phase(ProtocolPhases::SENDING_SIGNAL_LEVEL);
    return result;
  } else {
    this->set_phase(ProtocolPhases::IDLE);
    return result;
  }
}

haier_protocol::HandlerError HonClimate::report_network_status_answer_handler_(uint8_t request_type,
                                                                               uint8_t message_type,
                                                                               const uint8_t *data, size_t data_size) {
  haier_protocol::HandlerError result =
      this->answer_preprocess_(request_type, (uint8_t) hon_protocol::FrameType::REPORT_NETWORK_STATUS, message_type,
                               (uint8_t) hon_protocol::FrameType::CONFIRM, ProtocolPhases::WAITING_SIGNAL_LEVEL_ANSWER);
  this->set_phase(ProtocolPhases::IDLE);
  return result;
}

haier_protocol::HandlerError HonClimate::get_alarm_status_answer_handler_(uint8_t request_type, uint8_t message_type,
                                                                          const uint8_t *data, size_t data_size) {
  if (request_type == (uint8_t) hon_protocol::FrameType::GET_ALARM_STATUS) {
    if (message_type != (uint8_t) hon_protocol::FrameType::GET_ALARM_STATUS_RESPONSE) {
      // Unexpected answer to request
      this->set_phase(ProtocolPhases::IDLE);
      return haier_protocol::HandlerError::UNSUPPORTED_MESSAGE;
    }
    if (this->protocol_phase_ != ProtocolPhases::WAITING_ALARM_STATUS_ANSWER) {
      // Don't expect this answer now
      this->set_phase(ProtocolPhases::IDLE);
      return haier_protocol::HandlerError::UNEXPECTED_MESSAGE;
    }
    memcpy(this->active_alarms_, data + 2, 8);
    this->set_phase(ProtocolPhases::IDLE);
    return haier_protocol::HandlerError::HANDLER_OK;
  } else {
    this->set_phase(ProtocolPhases::IDLE);
    return haier_protocol::HandlerError::UNSUPPORTED_MESSAGE;
  }
}

void HonClimate::set_handlers() {
  // Set handlers
  this->haier_protocol_.set_answer_handler(
      (uint8_t) (hon_protocol::FrameType::GET_DEVICE_VERSION),
      std::bind(&HonClimate::get_device_version_answer_handler_, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      (uint8_t) (hon_protocol::FrameType::GET_DEVICE_ID),
      std::bind(&HonClimate::get_device_id_answer_handler_, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      (uint8_t) (hon_protocol::FrameType::CONTROL),
      std::bind(&HonClimate::status_handler_, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      (uint8_t) (hon_protocol::FrameType::GET_MANAGEMENT_INFORMATION),
      std::bind(&HonClimate::get_management_information_answer_handler_, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      (uint8_t) (hon_protocol::FrameType::GET_ALARM_STATUS),
      std::bind(&HonClimate::get_alarm_status_answer_handler_, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      (uint8_t) (hon_protocol::FrameType::REPORT_NETWORK_STATUS),
      std::bind(&HonClimate::report_network_status_answer_handler_, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4));
}

void HonClimate::dump_config() {
  HaierClimateBase::dump_config();
  ESP_LOGCONFIG(TAG, "  Protocol version: hOn");
  if (this->hvac_hardware_info_available_) {
    ESP_LOGCONFIG(TAG, "  Device protocol version: %s", this->hvac_protocol_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device software version: %s", this->hvac_software_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device hardware version: %s", this->hvac_hardware_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device name: %s", this->hvac_device_name_.c_str());
    ESP_LOGCONFIG(TAG, "  Device features:%s%s%s%s%s", (this->hvac_functions_[0] ? " interactive" : ""),
                  (this->hvac_functions_[1] ? " controller-device" : ""), (this->hvac_functions_[2] ? " crc" : ""),
                  (this->hvac_functions_[3] ? " multinode" : ""), (this->hvac_functions_[4] ? " role" : ""));
    ESP_LOGCONFIG(TAG, "  Active alarms: %s", buf_to_hex(this->active_alarms_, sizeof(this->active_alarms_)).c_str());
  }
}

void HonClimate::process_phase(std::chrono::steady_clock::time_point now) {
  switch (this->protocol_phase_) {
    case ProtocolPhases::SENDING_INIT_1:
      if (this->can_send_message() && this->is_protocol_initialisation_interval_exceeded_(now)) {
        this->hvac_hardware_info_available_ = false;
        // Indicate device capabilities:
        // bit 0 - if 1 module support interactive mode
        // bit 1 - if 1 module support controller-device mode
        // bit 2 - if 1 module support crc
        // bit 3 - if 1 module support multiple devices
        // bit 4..bit 15 - not used
        uint8_t module_capabilities[2] = {0b00000000, 0b00000111};
        static const haier_protocol::HaierMessage DEVICE_VERSION_REQUEST(
            (uint8_t) hon_protocol::FrameType::GET_DEVICE_VERSION, module_capabilities, sizeof(module_capabilities));
        this->send_message_(DEVICE_VERSION_REQUEST, this->use_crc_);
        this->set_phase(ProtocolPhases::WAITING_INIT_1_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_INIT_2:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        static const haier_protocol::HaierMessage DEVICEID_REQUEST((uint8_t) hon_protocol::FrameType::GET_DEVICE_ID);
        this->send_message_(DEVICEID_REQUEST, this->use_crc_);
        this->set_phase(ProtocolPhases::WAITING_INIT_2_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_FIRST_STATUS_REQUEST:
    case ProtocolPhases::SENDING_STATUS_REQUEST:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        static const haier_protocol::HaierMessage STATUS_REQUEST(
            (uint8_t) hon_protocol::FrameType::CONTROL, (uint16_t) hon_protocol::SubcommandsControl::GET_USER_DATA);
        this->send_message_(STATUS_REQUEST, this->use_crc_);
        this->last_status_request_ = now;
        this->set_phase((ProtocolPhases) ((uint8_t) this->protocol_phase_ + 1));
      }
      break;
#ifdef USE_WIFI
    case ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        static const haier_protocol::HaierMessage UPDATE_SIGNAL_REQUEST(
            (uint8_t) hon_protocol::FrameType::GET_MANAGEMENT_INFORMATION);
        this->send_message_(UPDATE_SIGNAL_REQUEST, this->use_crc_);
        this->last_signal_request_ = now;
        this->set_phase(ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_SIGNAL_LEVEL:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        this->send_message_(this->get_wifi_signal_message_((uint8_t) hon_protocol::FrameType::REPORT_NETWORK_STATUS),
                            this->use_crc_);
        this->set_phase(ProtocolPhases::WAITING_SIGNAL_LEVEL_ANSWER);
      }
      break;
    case ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER:
    case ProtocolPhases::WAITING_SIGNAL_LEVEL_ANSWER:
      break;
#else
    case ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST:
    case ProtocolPhases::SENDING_SIGNAL_LEVEL:
    case ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER:
    case ProtocolPhases::WAITING_SIGNAL_LEVEL_ANSWER:
      this->set_phase(ProtocolPhases::IDLE);
      break;
#endif
    case ProtocolPhases::SENDING_ALARM_STATUS_REQUEST:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        static const haier_protocol::HaierMessage ALARM_STATUS_REQUEST(
            (uint8_t) hon_protocol::FrameType::GET_ALARM_STATUS);
        this->send_message_(ALARM_STATUS_REQUEST, this->use_crc_);
        this->set_phase(ProtocolPhases::WAITING_ALARM_STATUS_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_CONTROL:
      if (this->first_control_attempt_) {
        this->control_request_timestamp_ = now;
        this->first_control_attempt_ = false;
      }
      if (this->is_control_message_timeout_exceeded_(now)) {
        ESP_LOGW(TAG, "Sending control packet timeout!");
        this->set_force_send_control_(false);
        if (this->hvac_settings_.valid)
          this->hvac_settings_.reset();
        this->forced_request_status_ = true;
        this->forced_publish_ = true;
        this->set_phase(ProtocolPhases::IDLE);
      } else if (this->can_send_message() && this->is_control_message_interval_exceeded_(now)) {
        haier_protocol::HaierMessage control_message = get_control_message();
        this->send_message_(control_message, this->use_crc_);
        ESP_LOGI(TAG, "Control packet sent");
        this->set_phase(ProtocolPhases::WAITING_CONTROL_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_POWER_ON_COMMAND:
    case ProtocolPhases::SENDING_POWER_OFF_COMMAND:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        uint8_t pwr_cmd_buf[2] = {0x00, 0x00};
        if (this->protocol_phase_ == ProtocolPhases::SENDING_POWER_ON_COMMAND)
          pwr_cmd_buf[1] = 0x01;
        haier_protocol::HaierMessage power_cmd((uint8_t) hon_protocol::FrameType::CONTROL,
                                               ((uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER) + 1,
                                               pwr_cmd_buf, sizeof(pwr_cmd_buf));
        this->send_message_(power_cmd, this->use_crc_);
        this->set_phase(this->protocol_phase_ == ProtocolPhases::SENDING_POWER_ON_COMMAND
                            ? ProtocolPhases::WAITING_POWER_ON_ANSWER
                            : ProtocolPhases::WAITING_POWER_OFF_ANSWER);
      }
      break;

    case ProtocolPhases::WAITING_INIT_1_ANSWER:
    case ProtocolPhases::WAITING_INIT_2_ANSWER:
    case ProtocolPhases::WAITING_FIRST_STATUS_ANSWER:
    case ProtocolPhases::WAITING_ALARM_STATUS_ANSWER:
    case ProtocolPhases::WAITING_STATUS_ANSWER:
    case ProtocolPhases::WAITING_CONTROL_ANSWER:
    case ProtocolPhases::WAITING_POWER_ON_ANSWER:
    case ProtocolPhases::WAITING_POWER_OFF_ANSWER:
      break;
    case ProtocolPhases::IDLE: {
      if (this->forced_request_status_ || this->is_status_request_interval_exceeded_(now)) {
        this->set_phase(ProtocolPhases::SENDING_STATUS_REQUEST);
        this->forced_request_status_ = false;
      }
#ifdef USE_WIFI
      else if (this->send_wifi_signal_ &&
               (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_signal_request_).count() >
                SIGNAL_LEVEL_UPDATE_INTERVAL_MS))
        this->set_phase(ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST);
#endif
    } break;
    default:
      // Shouldn't get here
#if (HAIER_LOG_LEVEL > 4)
      ESP_LOGE(TAG, "Wrong protocol handler state: %s (%d), resetting communication",
               phase_to_string_(this->protocol_phase_), (int) this->protocol_phase_);
#else
      ESP_LOGE(TAG, "Wrong protocol handler state: %d, resetting communication", (int) this->protocol_phase_);
#endif
      this->set_phase(ProtocolPhases::SENDING_INIT_1);
      break;
  }
}

haier_protocol::HaierMessage HonClimate::get_control_message() {
  uint8_t control_out_buffer[sizeof(hon_protocol::HaierPacketControl)];
  memcpy(control_out_buffer, this->last_status_message_.get(), sizeof(hon_protocol::HaierPacketControl));
  hon_protocol::HaierPacketControl *out_data = (hon_protocol::HaierPacketControl *) control_out_buffer;
  bool has_hvac_settings = false;
  if (this->hvac_settings_.valid) {
    has_hvac_settings = true;
    HvacSettings climate_control;
    climate_control = this->hvac_settings_;
    if (climate_control.mode.has_value()) {
      switch (climate_control.mode.value()) {
        case CLIMATE_MODE_OFF:
          out_data->ac_power = 0;
          break;
        case CLIMATE_MODE_HEAT_COOL:
          out_data->ac_power = 1;
          out_data->ac_mode = (uint8_t) hon_protocol::ConditioningMode::AUTO;
          out_data->fan_mode = this->other_modes_fan_speed_;
          break;
        case CLIMATE_MODE_HEAT:
          out_data->ac_power = 1;
          out_data->ac_mode = (uint8_t) hon_protocol::ConditioningMode::HEAT;
          out_data->fan_mode = this->other_modes_fan_speed_;
          break;
        case CLIMATE_MODE_DRY:
          out_data->ac_power = 1;
          out_data->ac_mode = (uint8_t) hon_protocol::ConditioningMode::DRY;
          out_data->fan_mode = this->other_modes_fan_speed_;
          break;
        case CLIMATE_MODE_FAN_ONLY:
          out_data->ac_power = 1;
          out_data->ac_mode = (uint8_t) hon_protocol::ConditioningMode::FAN;
          out_data->fan_mode = this->fan_mode_speed_;  // Auto doesn't work in fan only mode
          // Disabling boost and eco mode for Fan only
          out_data->quiet_mode = 0;
          out_data->fast_mode = 0;
          break;
        case CLIMATE_MODE_COOL:
          out_data->ac_power = 1;
          out_data->ac_mode = (uint8_t) hon_protocol::ConditioningMode::COOL;
          out_data->fan_mode = this->other_modes_fan_speed_;
          break;
        default:
          ESP_LOGE("Control", "Unsupported climate mode");
          break;
      }
    }
    // Set fan speed, if we are in fan mode, reject auto in fan mode
    if (climate_control.fan_mode.has_value()) {
      switch (climate_control.fan_mode.value()) {
        case CLIMATE_FAN_LOW:
          out_data->fan_mode = (uint8_t) hon_protocol::FanMode::FAN_LOW;
          break;
        case CLIMATE_FAN_MEDIUM:
          out_data->fan_mode = (uint8_t) hon_protocol::FanMode::FAN_MID;
          break;
        case CLIMATE_FAN_HIGH:
          out_data->fan_mode = (uint8_t) hon_protocol::FanMode::FAN_HIGH;
          break;
        case CLIMATE_FAN_AUTO:
          if (mode != CLIMATE_MODE_FAN_ONLY)  // if we are not in fan only mode
            out_data->fan_mode = (uint8_t) hon_protocol::FanMode::FAN_AUTO;
          break;
        default:
          ESP_LOGE("Control", "Unsupported fan mode");
          break;
      }
    }
    // Set swing mode
    if (climate_control.swing_mode.has_value()) {
      switch (climate_control.swing_mode.value()) {
        case CLIMATE_SWING_OFF:
          out_data->horizontal_swing_mode = (uint8_t) get_horizontal_swing_mode(this->horizontal_direction_);
          out_data->vertical_swing_mode = (uint8_t) get_vertical_swing_mode(this->vertical_direction_);
          break;
        case CLIMATE_SWING_VERTICAL:
          out_data->horizontal_swing_mode = (uint8_t) get_horizontal_swing_mode(this->horizontal_direction_);
          out_data->vertical_swing_mode = (uint8_t) hon_protocol::VerticalSwingMode::AUTO;
          break;
        case CLIMATE_SWING_HORIZONTAL:
          out_data->horizontal_swing_mode = (uint8_t) hon_protocol::HorizontalSwingMode::AUTO;
          out_data->vertical_swing_mode = (uint8_t) get_vertical_swing_mode(this->vertical_direction_);
          break;
        case CLIMATE_SWING_BOTH:
          out_data->horizontal_swing_mode = (uint8_t) hon_protocol::HorizontalSwingMode::AUTO;
          out_data->vertical_swing_mode = (uint8_t) hon_protocol::VerticalSwingMode::AUTO;
          break;
      }
    }
    if (climate_control.target_temperature.has_value()) {
      float target_temp = climate_control.target_temperature.value();
      out_data->set_point = ((int) target_temp) - 16;  // set the temperature at our offset, subtract 16.
      out_data->half_degree = (target_temp - ((int) target_temp) >= 0.49) ? 1 : 0;
    }
    if (out_data->ac_power == 0) {
      // If AC is off - no presets allowed
      out_data->quiet_mode = 0;
      out_data->fast_mode = 0;
      out_data->sleep_mode = 0;
    } else if (climate_control.preset.has_value()) {
      switch (climate_control.preset.value()) {
        case CLIMATE_PRESET_NONE:
          out_data->quiet_mode = 0;
          out_data->fast_mode = 0;
          out_data->sleep_mode = 0;
          break;
        case CLIMATE_PRESET_ECO:
          // Eco is not supported in Fan only mode
          out_data->quiet_mode = (this->mode != CLIMATE_MODE_FAN_ONLY) ? 1 : 0;
          out_data->fast_mode = 0;
          out_data->sleep_mode = 0;
          break;
        case CLIMATE_PRESET_BOOST:
          out_data->quiet_mode = 0;
          // Boost is not supported in Fan only mode
          out_data->fast_mode = (this->mode != CLIMATE_MODE_FAN_ONLY) ? 1 : 0;
          out_data->sleep_mode = 0;
          break;
        case CLIMATE_PRESET_AWAY:
          out_data->quiet_mode = 0;
          out_data->fast_mode = 0;
          out_data->sleep_mode = 0;
          break;
        case CLIMATE_PRESET_SLEEP:
          out_data->quiet_mode = 0;
          out_data->fast_mode = 0;
          out_data->sleep_mode = 1;
          break;
        default:
          ESP_LOGE("Control", "Unsupported preset");
          break;
      }
    }
  } else {
    if (out_data->vertical_swing_mode != (uint8_t) hon_protocol::VerticalSwingMode::AUTO)
      out_data->vertical_swing_mode = (uint8_t) get_vertical_swing_mode(this->vertical_direction_);
    if (out_data->horizontal_swing_mode != (uint8_t) hon_protocol::HorizontalSwingMode::AUTO)
      out_data->horizontal_swing_mode = (uint8_t) get_horizontal_swing_mode(this->horizontal_direction_);
  }
  out_data->beeper_status = ((!this->beeper_status_) || (!has_hvac_settings)) ? 1 : 0;
  control_out_buffer[4] = 0;  // This byte should be cleared before setting values
  out_data->display_status = this->display_status_ ? 1 : 0;
  out_data->health_mode = this->health_mode_ ? 1 : 0;
  switch (this->action_request_) {
    case ActionRequest::START_SELF_CLEAN:
      this->action_request_ = ActionRequest::NO_ACTION;
      out_data->self_cleaning_status = 1;
      out_data->steri_clean = 0;
      out_data->set_point = 0x06;
      out_data->vertical_swing_mode = (uint8_t) hon_protocol::VerticalSwingMode::CENTER;
      out_data->horizontal_swing_mode = (uint8_t) hon_protocol::HorizontalSwingMode::CENTER;
      out_data->ac_power = 1;
      out_data->ac_mode = (uint8_t) hon_protocol::ConditioningMode::DRY;
      out_data->light_status = 0;
      break;
    case ActionRequest::START_STERI_CLEAN:
      this->action_request_ = ActionRequest::NO_ACTION;
      out_data->self_cleaning_status = 0;
      out_data->steri_clean = 1;
      out_data->set_point = 0x06;
      out_data->vertical_swing_mode = (uint8_t) hon_protocol::VerticalSwingMode::CENTER;
      out_data->horizontal_swing_mode = (uint8_t) hon_protocol::HorizontalSwingMode::CENTER;
      out_data->ac_power = 1;
      out_data->ac_mode = (uint8_t) hon_protocol::ConditioningMode::DRY;
      out_data->light_status = 0;
      break;
    default:
      // No change
      break;
  }
  return haier_protocol::HaierMessage((uint8_t) hon_protocol::FrameType::CONTROL,
                                      (uint16_t) hon_protocol::SubcommandsControl::SET_GROUP_PARAMETERS,
                                      control_out_buffer, sizeof(hon_protocol::HaierPacketControl));
}

haier_protocol::HandlerError HonClimate::process_status_message_(const uint8_t *packet_buffer, uint8_t size) {
  if (size < sizeof(hon_protocol::HaierStatus))
    return haier_protocol::HandlerError::WRONG_MESSAGE_STRUCTURE;
  hon_protocol::HaierStatus packet;
  if (size < sizeof(hon_protocol::HaierStatus))
    size = sizeof(hon_protocol::HaierStatus);
  memcpy(&packet, packet_buffer, size);
  if (packet.sensors.error_status != 0) {
    ESP_LOGW(TAG, "HVAC error, code=0x%02X", packet.sensors.error_status);
  }
  if ((this->outdoor_sensor_ != nullptr) && (got_valid_outdoor_temp_ || (packet.sensors.outdoor_temperature > 0))) {
    got_valid_outdoor_temp_ = true;
    float otemp = (float) (packet.sensors.outdoor_temperature + PROTOCOL_OUTDOOR_TEMPERATURE_OFFSET);
    if ((!this->outdoor_sensor_->has_state()) || (this->outdoor_sensor_->get_raw_state() != otemp))
      this->outdoor_sensor_->publish_state(otemp);
  }
  bool should_publish = false;
  {
    // Extra modes/presets
    optional<ClimatePreset> old_preset = this->preset;
    if (packet.control.quiet_mode != 0) {
      this->preset = CLIMATE_PRESET_ECO;
    } else if (packet.control.fast_mode != 0) {
      this->preset = CLIMATE_PRESET_BOOST;
    } else if (packet.control.sleep_mode != 0) {
      this->preset = CLIMATE_PRESET_SLEEP;
    } else {
      this->preset = CLIMATE_PRESET_NONE;
    }
    should_publish = should_publish || (!old_preset.has_value()) || (old_preset.value() != this->preset.value());
  }
  {
    // Target temperature
    float old_target_temperature = this->target_temperature;
    this->target_temperature = packet.control.set_point + 16.0f + ((packet.control.half_degree == 1) ? 0.5f : 0.0f);
    should_publish = should_publish || (old_target_temperature != this->target_temperature);
  }
  {
    // Current temperature
    float old_current_temperature = this->current_temperature;
    this->current_temperature = packet.sensors.room_temperature / 2.0f;
    should_publish = should_publish || (old_current_temperature != this->current_temperature);
  }
  {
    // Fan mode
    optional<ClimateFanMode> old_fan_mode = this->fan_mode;
    // remember the fan speed we last had for climate vs fan
    if (packet.control.ac_mode == (uint8_t) hon_protocol::ConditioningMode::FAN) {
      if (packet.control.fan_mode != (uint8_t) hon_protocol::FanMode::FAN_AUTO)
        this->fan_mode_speed_ = packet.control.fan_mode;
    } else {
      this->other_modes_fan_speed_ = packet.control.fan_mode;
    }
    switch (packet.control.fan_mode) {
      case (uint8_t) hon_protocol::FanMode::FAN_AUTO:
        if (packet.control.ac_mode != (uint8_t) hon_protocol::ConditioningMode::FAN) {
          this->fan_mode = CLIMATE_FAN_AUTO;
        } else {
          // Shouldn't accept fan speed auto in fan-only mode even if AC reports it
          ESP_LOGI(TAG, "Fan speed Auto is not supported in Fan only AC mode, ignoring");
        }
        break;
      case (uint8_t) hon_protocol::FanMode::FAN_MID:
        this->fan_mode = CLIMATE_FAN_MEDIUM;
        break;
      case (uint8_t) hon_protocol::FanMode::FAN_LOW:
        this->fan_mode = CLIMATE_FAN_LOW;
        break;
      case (uint8_t) hon_protocol::FanMode::FAN_HIGH:
        this->fan_mode = CLIMATE_FAN_HIGH;
        break;
    }
    should_publish = should_publish || (!old_fan_mode.has_value()) || (old_fan_mode.value() != fan_mode.value());
  }
  {
    // Display status
    // should be before "Climate mode" because it is changing this->mode
    if (packet.control.ac_power != 0) {
      // if AC is off display status always ON so process it only when AC is on
      bool disp_status = packet.control.display_status != 0;
      if (disp_status != this->display_status_) {
        // Do something only if display status changed
        if (this->mode == CLIMATE_MODE_OFF) {
          // AC just turned on from remote need to turn off display
          this->set_force_send_control_(true);
        } else {
          this->display_status_ = disp_status;
        }
      }
    }
  }
  {
    // Health mode
    bool old_health_mode = this->health_mode_;
    this->health_mode_ = packet.control.health_mode == 1;
    should_publish = should_publish || (old_health_mode != this->health_mode_);
  }
  {
    CleaningState new_cleaning;
    if (packet.control.steri_clean == 1) {
      // Steri-cleaning
      new_cleaning = CleaningState::STERI_CLEAN;
    } else if (packet.control.self_cleaning_status == 1) {
      // Self-cleaning
      new_cleaning = CleaningState::SELF_CLEAN;
    } else {
      // No cleaning
      new_cleaning = CleaningState::NO_CLEANING;
    }
    if (new_cleaning != this->cleaning_status_) {
      ESP_LOGD(TAG, "Cleaning status change: %d => %d", (uint8_t) this->cleaning_status_, (uint8_t) new_cleaning);
      if (new_cleaning == CleaningState::NO_CLEANING) {
        // Turning AC off after cleaning
        this->action_request_ = ActionRequest::TURN_POWER_OFF;
      }
      this->cleaning_status_ = new_cleaning;
    }
  }
  {
    // Climate mode
    ClimateMode old_mode = this->mode;
    if (packet.control.ac_power == 0) {
      this->mode = CLIMATE_MODE_OFF;
    } else {
      // Check current hvac mode
      switch (packet.control.ac_mode) {
        case (uint8_t) hon_protocol::ConditioningMode::COOL:
          this->mode = CLIMATE_MODE_COOL;
          break;
        case (uint8_t) hon_protocol::ConditioningMode::HEAT:
          this->mode = CLIMATE_MODE_HEAT;
          break;
        case (uint8_t) hon_protocol::ConditioningMode::DRY:
          this->mode = CLIMATE_MODE_DRY;
          break;
        case (uint8_t) hon_protocol::ConditioningMode::FAN:
          this->mode = CLIMATE_MODE_FAN_ONLY;
          break;
        case (uint8_t) hon_protocol::ConditioningMode::AUTO:
          this->mode = CLIMATE_MODE_HEAT_COOL;
          break;
      }
    }
    should_publish = should_publish || (old_mode != this->mode);
  }
  {
    // Swing mode
    ClimateSwingMode old_swing_mode = this->swing_mode;
    if (packet.control.horizontal_swing_mode == (uint8_t) hon_protocol::HorizontalSwingMode::AUTO) {
      if (packet.control.vertical_swing_mode == (uint8_t) hon_protocol::VerticalSwingMode::AUTO) {
        this->swing_mode = CLIMATE_SWING_BOTH;
      } else {
        this->swing_mode = CLIMATE_SWING_HORIZONTAL;
      }
    } else {
      if (packet.control.vertical_swing_mode == (uint8_t) hon_protocol::VerticalSwingMode::AUTO) {
        this->swing_mode = CLIMATE_SWING_VERTICAL;
      } else {
        this->swing_mode = CLIMATE_SWING_OFF;
      }
    }
    should_publish = should_publish || (old_swing_mode != this->swing_mode);
  }
  this->last_valid_status_timestamp_ = std::chrono::steady_clock::now();
  if (this->forced_publish_ || should_publish) {
#if (HAIER_LOG_LEVEL > 4)
    std::chrono::high_resolution_clock::time_point _publish_start = std::chrono::high_resolution_clock::now();
#endif
    this->publish_state();
#if (HAIER_LOG_LEVEL > 4)
    ESP_LOGV(TAG, "Publish delay: %lld ms",
             std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() -
                                                                   _publish_start)
                 .count());
#endif
    this->forced_publish_ = false;
  }
  if (should_publish) {
    ESP_LOGI(TAG, "HVAC values changed");
  }
  esp_log_printf_((should_publish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__,
                  "HVAC Mode = 0x%X", packet.control.ac_mode);
  esp_log_printf_((should_publish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__,
                  "Fan speed Status = 0x%X", packet.control.fan_mode);
  esp_log_printf_((should_publish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__,
                  "Horizontal Swing Status = 0x%X", packet.control.horizontal_swing_mode);
  esp_log_printf_((should_publish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__,
                  "Vertical Swing Status = 0x%X", packet.control.vertical_swing_mode);
  esp_log_printf_((should_publish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__,
                  "Set Point Status = 0x%X", packet.control.set_point);
  return haier_protocol::HandlerError::HANDLER_OK;
}

bool HonClimate::is_message_invalid(uint8_t message_type) {
  return message_type == (uint8_t) hon_protocol::FrameType::INVALID;
}

void HonClimate::process_pending_action() {
  switch (this->action_request_) {
    case ActionRequest::START_SELF_CLEAN:
    case ActionRequest::START_STERI_CLEAN:
      // Will reset action with control message sending
      this->set_phase(ProtocolPhases::SENDING_CONTROL);
      break;
    default:
      HaierClimateBase::process_pending_action();
      break;
  }
}

}  // namespace haier
}  // namespace esphome
