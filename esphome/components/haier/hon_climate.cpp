#include <chrono>
#include <string>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "esphome/core/helpers.h"
#include "hon_climate.h"
#include "hon_packet.h"

using namespace esphome::climate;
using namespace esphome::uart;

namespace esphome {
namespace haier {

static const char *const TAG = "haier.climate";
constexpr size_t SIGNAL_LEVEL_UPDATE_INTERVAL_MS = 10000;
constexpr int PROTOCOL_OUTDOOR_TEMPERATURE_OFFSET = -64;
constexpr uint8_t CONTROL_MESSAGE_RETRIES = 5;
constexpr std::chrono::milliseconds CONTROL_MESSAGE_RETRIES_INTERVAL = std::chrono::milliseconds(500);
constexpr size_t ALARM_STATUS_REQUEST_INTERVAL_MS = 600000;

HonClimate::HonClimate()
    : cleaning_status_(CleaningState::NO_CLEANING), got_valid_outdoor_temp_(false), active_alarms_{0x00, 0x00, 0x00,
                                                                                                   0x00, 0x00, 0x00,
                                                                                                   0x00, 0x00} {
  last_status_message_ = std::unique_ptr<uint8_t[]>(new uint8_t[sizeof(hon_protocol::HaierPacketControl)]);
  this->fan_mode_speed_ = (uint8_t) hon_protocol::FanMode::FAN_MID;
  this->other_modes_fan_speed_ = (uint8_t) hon_protocol::FanMode::FAN_AUTO;
}

HonClimate::~HonClimate() {}

void HonClimate::set_beeper_state(bool state) { this->beeper_status_ = state; }

bool HonClimate::get_beeper_state() const { return this->beeper_status_; }

esphome::optional<hon_protocol::VerticalSwingMode> HonClimate::get_vertical_airflow() const {
  return this->current_vertical_swing_;
};

void HonClimate::set_vertical_airflow(hon_protocol::VerticalSwingMode direction) {
  this->pending_vertical_direction_ = direction;
  this->force_send_control_ = true;
}

esphome::optional<hon_protocol::HorizontalSwingMode> HonClimate::get_horizontal_airflow() const {
  return this->current_horizontal_swing_;
}

void HonClimate::set_horizontal_airflow(hon_protocol::HorizontalSwingMode direction) {
  this->pending_horizontal_direction_ = direction;
  this->force_send_control_ = true;
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
    this->action_request_ =
        PendingAction({ActionRequest::START_SELF_CLEAN, esphome::optional<haier_protocol::HaierMessage>()});
  }
}

void HonClimate::start_steri_cleaning() {
  if (this->cleaning_status_ == CleaningState::NO_CLEANING) {
    ESP_LOGI(TAG, "Sending steri cleaning start request");
    this->action_request_ =
        PendingAction({ActionRequest::START_STERI_CLEAN, esphome::optional<haier_protocol::HaierMessage>()});
  }
}

void HonClimate::add_alarm_start_callback(std::function<void(uint8_t, const char *)> &&callback) {
  this->alarm_start_callback_.add(std::move(callback));
}

void HonClimate::add_alarm_end_callback(std::function<void(uint8_t, const char *)> &&callback) {
  this->alarm_end_callback_.add(std::move(callback));
}

haier_protocol::HandlerError HonClimate::get_device_version_answer_handler_(haier_protocol::FrameType request_type,
                                                                            haier_protocol::FrameType message_type,
                                                                            const uint8_t *data, size_t data_size) {
  // Should check this before preprocess
  if (message_type == haier_protocol::FrameType::INVALID) {
    ESP_LOGW(TAG, "It looks like your ESPHome Haier climate configuration is wrong. You should use the smartAir2 "
                  "protocol instead of hOn");
    this->set_phase(ProtocolPhases::SENDING_INIT_1);
    return haier_protocol::HandlerError::INVALID_ANSWER;
  }
  haier_protocol::HandlerError result =
      this->answer_preprocess_(request_type, haier_protocol::FrameType::GET_DEVICE_VERSION, message_type,
                               haier_protocol::FrameType::GET_DEVICE_VERSION_RESPONSE, ProtocolPhases::SENDING_INIT_1);
  if (result == haier_protocol::HandlerError::HANDLER_OK) {
    if (data_size < sizeof(hon_protocol::DeviceVersionAnswer)) {
      // Wrong structure
      return haier_protocol::HandlerError::WRONG_MESSAGE_STRUCTURE;
    }
    // All OK
    hon_protocol::DeviceVersionAnswer *answr = (hon_protocol::DeviceVersionAnswer *) data;
    char tmp[9];
    tmp[8] = 0;
    strncpy(tmp, answr->protocol_version, 8);
    this->hvac_hardware_info_ = HardwareInfo();
    this->hvac_hardware_info_.value().protocol_version_ = std::string(tmp);
    strncpy(tmp, answr->software_version, 8);
    this->hvac_hardware_info_.value().software_version_ = std::string(tmp);
    strncpy(tmp, answr->hardware_version, 8);
    this->hvac_hardware_info_.value().hardware_version_ = std::string(tmp);
    strncpy(tmp, answr->device_name, 8);
    this->hvac_hardware_info_.value().device_name_ = std::string(tmp);
#ifdef USE_TEXT_SENSOR
    this->update_sub_text_sensor_(SubTextSensorType::APPLIANCE_NAME, this->hvac_hardware_info_.value().device_name_);
    this->update_sub_text_sensor_(SubTextSensorType::PROTOCOL_VERSION,
                                  this->hvac_hardware_info_.value().protocol_version_);
#endif
    this->hvac_hardware_info_.value().functions_[0] = (answr->functions[1] & 0x01) != 0;  // interactive mode support
    this->hvac_hardware_info_.value().functions_[1] =
        (answr->functions[1] & 0x02) != 0;  // controller-device mode support
    this->hvac_hardware_info_.value().functions_[2] = (answr->functions[1] & 0x04) != 0;  // crc support
    this->hvac_hardware_info_.value().functions_[3] = (answr->functions[1] & 0x08) != 0;  // multiple AC support
    this->hvac_hardware_info_.value().functions_[4] = (answr->functions[1] & 0x20) != 0;  // roles support
    this->use_crc_ = this->hvac_hardware_info_.value().functions_[2];
    this->set_phase(ProtocolPhases::SENDING_INIT_2);
    return result;
  } else {
    this->reset_phase_();
    return result;
  }
}

haier_protocol::HandlerError HonClimate::get_device_id_answer_handler_(haier_protocol::FrameType request_type,
                                                                       haier_protocol::FrameType message_type,
                                                                       const uint8_t *data, size_t data_size) {
  haier_protocol::HandlerError result =
      this->answer_preprocess_(request_type, haier_protocol::FrameType::GET_DEVICE_ID, message_type,
                               haier_protocol::FrameType::GET_DEVICE_ID_RESPONSE, ProtocolPhases::SENDING_INIT_2);
  if (result == haier_protocol::HandlerError::HANDLER_OK) {
    this->set_phase(ProtocolPhases::SENDING_FIRST_STATUS_REQUEST);
    return result;
  } else {
    this->reset_phase_();
    return result;
  }
}

haier_protocol::HandlerError HonClimate::status_handler_(haier_protocol::FrameType request_type,
                                                         haier_protocol::FrameType message_type, const uint8_t *data,
                                                         size_t data_size) {
  haier_protocol::HandlerError result =
      this->answer_preprocess_(request_type, haier_protocol::FrameType::CONTROL, message_type,
                               haier_protocol::FrameType::STATUS, ProtocolPhases::UNKNOWN);
  if (result == haier_protocol::HandlerError::HANDLER_OK) {
    result = this->process_status_message_(data, data_size);
    if (result != haier_protocol::HandlerError::HANDLER_OK) {
      ESP_LOGW(TAG, "Error %d while parsing Status packet", (int) result);
      this->reset_phase_();
      this->action_request_.reset();
      this->force_send_control_ = false;
    } else {
      if (data_size >= sizeof(hon_protocol::HaierPacketControl) + 2) {
        memcpy(this->last_status_message_.get(), data + 2, sizeof(hon_protocol::HaierPacketControl));
      } else {
        ESP_LOGW(TAG, "Status packet too small: %d (should be >= %d)", data_size,
                 sizeof(hon_protocol::HaierPacketControl));
      }
      switch (this->protocol_phase_) {
        case ProtocolPhases::SENDING_FIRST_STATUS_REQUEST:
          ESP_LOGI(TAG, "First HVAC status received");
          this->set_phase(ProtocolPhases::SENDING_FIRST_ALARM_STATUS_REQUEST);
          break;
        case ProtocolPhases::SENDING_ACTION_COMMAND:
          // Do nothing, phase will be changed in process_phase
          break;
        case ProtocolPhases::SENDING_STATUS_REQUEST:
          this->set_phase(ProtocolPhases::IDLE);
          break;
        case ProtocolPhases::SENDING_CONTROL:
          if (!this->control_messages_queue_.empty())
            this->control_messages_queue_.pop();
          if (this->control_messages_queue_.empty()) {
            this->set_phase(ProtocolPhases::IDLE);
            this->force_send_control_ = false;
            if (this->current_hvac_settings_.valid)
              this->current_hvac_settings_.reset();
          } else {
            this->set_phase(ProtocolPhases::SENDING_CONTROL);
          }
          break;
        default:
          break;
      }
    }
    return result;
  } else {
    this->action_request_.reset();
    this->force_send_control_ = false;
    this->reset_phase_();
    return result;
  }
}

haier_protocol::HandlerError HonClimate::get_management_information_answer_handler_(
    haier_protocol::FrameType request_type, haier_protocol::FrameType message_type, const uint8_t *data,
    size_t data_size) {
  haier_protocol::HandlerError result = this->answer_preprocess_(
      request_type, haier_protocol::FrameType::GET_MANAGEMENT_INFORMATION, message_type,
      haier_protocol::FrameType::GET_MANAGEMENT_INFORMATION_RESPONSE, ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST);
  if (result == haier_protocol::HandlerError::HANDLER_OK) {
    this->set_phase(ProtocolPhases::SENDING_SIGNAL_LEVEL);
    return result;
  } else {
    this->set_phase(ProtocolPhases::IDLE);
    return result;
  }
}

haier_protocol::HandlerError HonClimate::get_alarm_status_answer_handler_(haier_protocol::FrameType request_type,
                                                                          haier_protocol::FrameType message_type,
                                                                          const uint8_t *data, size_t data_size) {
  if (request_type == haier_protocol::FrameType::GET_ALARM_STATUS) {
    if (message_type != haier_protocol::FrameType::GET_ALARM_STATUS_RESPONSE) {
      // Unexpected answer to request
      this->set_phase(ProtocolPhases::IDLE);
      return haier_protocol::HandlerError::UNSUPPORTED_MESSAGE;
    }
    if ((this->protocol_phase_ != ProtocolPhases::SENDING_FIRST_ALARM_STATUS_REQUEST) &&
        (this->protocol_phase_ != ProtocolPhases::SENDING_ALARM_STATUS_REQUEST)) {
      // Don't expect this answer now
      this->set_phase(ProtocolPhases::IDLE);
      return haier_protocol::HandlerError::UNEXPECTED_MESSAGE;
    }
    if (data_size < sizeof(active_alarms_) + 2)
      return haier_protocol::HandlerError::WRONG_MESSAGE_STRUCTURE;
    this->process_alarm_message_(data, data_size, this->protocol_phase_ >= ProtocolPhases::IDLE);
    this->set_phase(ProtocolPhases::IDLE);
    return haier_protocol::HandlerError::HANDLER_OK;
  } else {
    this->set_phase(ProtocolPhases::IDLE);
    return haier_protocol::HandlerError::UNSUPPORTED_MESSAGE;
  }
}

haier_protocol::HandlerError HonClimate::alarm_status_message_handler_(haier_protocol::FrameType type,
                                                                       const uint8_t *buffer, size_t size) {
  haier_protocol::HandlerError result = haier_protocol::HandlerError::HANDLER_OK;
  if (size < sizeof(this->active_alarms_) + 2) {
    // Log error but confirm anyway to avoid to many messages
    result = haier_protocol::HandlerError::WRONG_MESSAGE_STRUCTURE;
  }
  this->process_alarm_message_(buffer, size, true);
  this->haier_protocol_.send_answer(haier_protocol::HaierMessage(haier_protocol::FrameType::CONFIRM));
  this->last_alarm_request_ = std::chrono::steady_clock::now();
  return result;
}

void HonClimate::set_handlers() {
  // Set handlers
  this->haier_protocol_.set_answer_handler(
      haier_protocol::FrameType::GET_DEVICE_VERSION,
      std::bind(&HonClimate::get_device_version_answer_handler_, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      haier_protocol::FrameType::GET_DEVICE_ID,
      std::bind(&HonClimate::get_device_id_answer_handler_, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      haier_protocol::FrameType::CONTROL,
      std::bind(&HonClimate::status_handler_, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3,
                std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      haier_protocol::FrameType::GET_MANAGEMENT_INFORMATION,
      std::bind(&HonClimate::get_management_information_answer_handler_, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      haier_protocol::FrameType::GET_ALARM_STATUS,
      std::bind(&HonClimate::get_alarm_status_answer_handler_, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      haier_protocol::FrameType::REPORT_NETWORK_STATUS,
      std::bind(&HonClimate::report_network_status_answer_handler_, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_message_handler(
      haier_protocol::FrameType::ALARM_STATUS,
      std::bind(&HonClimate::alarm_status_message_handler_, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3));
}

void HonClimate::dump_config() {
  HaierClimateBase::dump_config();
  ESP_LOGCONFIG(TAG, "  Protocol version: hOn");
  ESP_LOGCONFIG(TAG, "  Control method: %d", (uint8_t) this->control_method_);
  if (this->hvac_hardware_info_.has_value()) {
    ESP_LOGCONFIG(TAG, "  Device protocol version: %s", this->hvac_hardware_info_.value().protocol_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device software version: %s", this->hvac_hardware_info_.value().software_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device hardware version: %s", this->hvac_hardware_info_.value().hardware_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device name: %s", this->hvac_hardware_info_.value().device_name_.c_str());
    ESP_LOGCONFIG(TAG, "  Device features:%s%s%s%s%s",
                  (this->hvac_hardware_info_.value().functions_[0] ? " interactive" : ""),
                  (this->hvac_hardware_info_.value().functions_[1] ? " controller-device" : ""),
                  (this->hvac_hardware_info_.value().functions_[2] ? " crc" : ""),
                  (this->hvac_hardware_info_.value().functions_[3] ? " multinode" : ""),
                  (this->hvac_hardware_info_.value().functions_[4] ? " role" : ""));
    ESP_LOGCONFIG(TAG, "  Active alarms: %s", buf_to_hex(this->active_alarms_, sizeof(this->active_alarms_)).c_str());
  }
}

void HonClimate::process_phase(std::chrono::steady_clock::time_point now) {
  switch (this->protocol_phase_) {
    case ProtocolPhases::SENDING_INIT_1:
      if (this->can_send_message() && this->is_protocol_initialisation_interval_exceeded_(now)) {
        // Indicate device capabilities:
        // bit 0 - if 1 module support interactive mode
        // bit 1 - if 1 module support controller-device mode
        // bit 2 - if 1 module support crc
        // bit 3 - if 1 module support multiple devices
        // bit 4..bit 15 - not used
        uint8_t module_capabilities[2] = {0b00000000, 0b00000111};
        static const haier_protocol::HaierMessage DEVICE_VERSION_REQUEST(
            haier_protocol::FrameType::GET_DEVICE_VERSION, module_capabilities, sizeof(module_capabilities));
        this->send_message_(DEVICE_VERSION_REQUEST, this->use_crc_);
      }
      break;
    case ProtocolPhases::SENDING_INIT_2:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        static const haier_protocol::HaierMessage DEVICEID_REQUEST(haier_protocol::FrameType::GET_DEVICE_ID);
        this->send_message_(DEVICEID_REQUEST, this->use_crc_);
      }
      break;
    case ProtocolPhases::SENDING_FIRST_STATUS_REQUEST:
    case ProtocolPhases::SENDING_STATUS_REQUEST:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        static const haier_protocol::HaierMessage STATUS_REQUEST(
            haier_protocol::FrameType::CONTROL, (uint16_t) hon_protocol::SubcommandsControl::GET_USER_DATA);
        static const haier_protocol::HaierMessage BIG_DATA_REQUEST(
            haier_protocol::FrameType::CONTROL, (uint16_t) hon_protocol::SubcommandsControl::GET_BIG_DATA);
        if ((this->protocol_phase_ == ProtocolPhases::SENDING_FIRST_STATUS_REQUEST) ||
            (!this->should_get_big_data_())) {
          this->send_message_(STATUS_REQUEST, this->use_crc_);
        } else {
          this->send_message_(BIG_DATA_REQUEST, this->use_crc_);
        }
        this->last_status_request_ = now;
      }
      break;
#ifdef USE_WIFI
    case ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        static const haier_protocol::HaierMessage UPDATE_SIGNAL_REQUEST(
            haier_protocol::FrameType::GET_MANAGEMENT_INFORMATION);
        this->send_message_(UPDATE_SIGNAL_REQUEST, this->use_crc_);
        this->last_signal_request_ = now;
      }
      break;
    case ProtocolPhases::SENDING_SIGNAL_LEVEL:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        this->send_message_(this->get_wifi_signal_message_(), this->use_crc_);
      }
      break;
#else
    case ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST:
    case ProtocolPhases::SENDING_SIGNAL_LEVEL:
      this->set_phase(ProtocolPhases::IDLE);
      break;
#endif
    case ProtocolPhases::SENDING_FIRST_ALARM_STATUS_REQUEST:
    case ProtocolPhases::SENDING_ALARM_STATUS_REQUEST:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        static const haier_protocol::HaierMessage ALARM_STATUS_REQUEST(haier_protocol::FrameType::GET_ALARM_STATUS);
        this->send_message_(ALARM_STATUS_REQUEST, this->use_crc_);
        this->last_alarm_request_ = now;
      }
      break;
    case ProtocolPhases::SENDING_CONTROL:
      if (this->control_messages_queue_.empty()) {
        switch (this->control_method_) {
          case HonControlMethod::SET_GROUP_PARAMETERS: {
            haier_protocol::HaierMessage control_message = this->get_control_message();
            this->control_messages_queue_.push(control_message);
          } break;
          case HonControlMethod::SET_SINGLE_PARAMETER:
            this->fill_control_messages_queue_();
            break;
          case HonControlMethod::MONITOR_ONLY:
            ESP_LOGI(TAG, "AC control is disabled, monitor only");
            this->reset_to_idle_();
            return;
          default:
            ESP_LOGW(TAG, "Unsupported control method for hOn protocol!");
            this->reset_to_idle_();
            return;
        }
      }
      if (this->control_messages_queue_.empty()) {
        ESP_LOGW(TAG, "Control message queue is empty!");
        this->reset_to_idle_();
      } else if (this->can_send_message() && this->is_control_message_interval_exceeded_(now)) {
        ESP_LOGI(TAG, "Sending control packet, queue size %d", this->control_messages_queue_.size());
        this->send_message_(this->control_messages_queue_.front(), this->use_crc_, CONTROL_MESSAGE_RETRIES,
                            CONTROL_MESSAGE_RETRIES_INTERVAL);
      }
      break;
    case ProtocolPhases::SENDING_ACTION_COMMAND:
      if (this->action_request_.has_value()) {
        if (this->action_request_.value().message.has_value()) {
          this->send_message_(this->action_request_.value().message.value(), this->use_crc_);
          this->action_request_.value().message.reset();
        } else {
          // Message already sent, reseting request and return to idle
          this->action_request_.reset();
          this->set_phase(ProtocolPhases::IDLE);
        }
      } else {
        ESP_LOGW(TAG, "SENDING_ACTION_COMMAND phase without action request!");
        this->set_phase(ProtocolPhases::IDLE);
      }
      break;
    case ProtocolPhases::IDLE: {
      if (this->forced_request_status_ || this->is_status_request_interval_exceeded_(now)) {
        this->set_phase(ProtocolPhases::SENDING_STATUS_REQUEST);
        this->forced_request_status_ = false;
      } else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_alarm_request_).count() >
                 ALARM_STATUS_REQUEST_INTERVAL_MS) {
        this->set_phase(ProtocolPhases::SENDING_ALARM_STATUS_REQUEST);
      }
#ifdef USE_WIFI
      else if (this->send_wifi_signal_ &&
               (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_signal_request_).count() >
                SIGNAL_LEVEL_UPDATE_INTERVAL_MS)) {
        this->set_phase(ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST);
      }
#endif
    } break;
    default:
      // Shouldn't get here
      ESP_LOGE(TAG, "Wrong protocol handler state: %s (%d), resetting communication",
               phase_to_string_(this->protocol_phase_), (int) this->protocol_phase_);
      this->set_phase(ProtocolPhases::SENDING_INIT_1);
      break;
  }
}

haier_protocol::HaierMessage HonClimate::get_power_message(bool state) {
  if (state) {
    static haier_protocol::HaierMessage power_on_message(
        haier_protocol::FrameType::CONTROL, ((uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER) + 1,
        std::initializer_list<uint8_t>({0x00, 0x01}).begin(), 2);
    return power_on_message;
  } else {
    static haier_protocol::HaierMessage power_off_message(
        haier_protocol::FrameType::CONTROL, ((uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER) + 1,
        std::initializer_list<uint8_t>({0x00, 0x00}).begin(), 2);
    return power_off_message;
  }
}

void HonClimate::initialization() {
  constexpr uint32_t restore_settings_version = 0xE834D8DCUL;
  this->rtc_ = global_preferences->make_preference<HonSettings>(this->get_object_id_hash() ^ restore_settings_version);
  HonSettings recovered;
  if (this->rtc_.load(&recovered)) {
    this->settings_ = recovered;
  } else {
    this->settings_ = {hon_protocol::VerticalSwingMode::CENTER, hon_protocol::HorizontalSwingMode::CENTER};
  }
  this->current_vertical_swing_ = this->settings_.last_vertiacal_swing;
  this->current_horizontal_swing_ = this->settings_.last_horizontal_swing;
}

haier_protocol::HaierMessage HonClimate::get_control_message() {
  uint8_t control_out_buffer[sizeof(hon_protocol::HaierPacketControl)];
  memcpy(control_out_buffer, this->last_status_message_.get(), sizeof(hon_protocol::HaierPacketControl));
  hon_protocol::HaierPacketControl *out_data = (hon_protocol::HaierPacketControl *) control_out_buffer;
  control_out_buffer[4] = 0;  // This byte should be cleared before setting values
  bool has_hvac_settings = false;
  if (this->current_hvac_settings_.valid) {
    has_hvac_settings = true;
    HvacSettings &climate_control = this->current_hvac_settings_;
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
          out_data->horizontal_swing_mode = (uint8_t) this->settings_.last_horizontal_swing;
          out_data->vertical_swing_mode = (uint8_t) this->settings_.last_vertiacal_swing;
          break;
        case CLIMATE_SWING_VERTICAL:
          out_data->horizontal_swing_mode = (uint8_t) this->settings_.last_horizontal_swing;
          out_data->vertical_swing_mode = (uint8_t) hon_protocol::VerticalSwingMode::AUTO;
          break;
        case CLIMATE_SWING_HORIZONTAL:
          out_data->horizontal_swing_mode = (uint8_t) hon_protocol::HorizontalSwingMode::AUTO;
          out_data->vertical_swing_mode = (uint8_t) this->settings_.last_vertiacal_swing;
          break;
        case CLIMATE_SWING_BOTH:
          out_data->horizontal_swing_mode = (uint8_t) hon_protocol::HorizontalSwingMode::AUTO;
          out_data->vertical_swing_mode = (uint8_t) hon_protocol::VerticalSwingMode::AUTO;
          break;
      }
    }
    if (climate_control.target_temperature.has_value()) {
      float target_temp = climate_control.target_temperature.value();
      out_data->set_point = ((int) target_temp) - 16;  // set the temperature with offset 16
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
          out_data->ten_degree = 0;
          break;
        case CLIMATE_PRESET_ECO:
          // Eco is not supported in Fan only mode
          out_data->quiet_mode = (this->mode != CLIMATE_MODE_FAN_ONLY) ? 1 : 0;
          out_data->fast_mode = 0;
          out_data->sleep_mode = 0;
          out_data->ten_degree = 0;
          break;
        case CLIMATE_PRESET_BOOST:
          out_data->quiet_mode = 0;
          // Boost is not supported in Fan only mode
          out_data->fast_mode = (this->mode != CLIMATE_MODE_FAN_ONLY) ? 1 : 0;
          out_data->sleep_mode = 0;
          out_data->ten_degree = 0;
          break;
        case CLIMATE_PRESET_AWAY:
          out_data->quiet_mode = 0;
          out_data->fast_mode = 0;
          out_data->sleep_mode = 0;
          // 10 degrees allowed only in heat mode
          out_data->ten_degree = (this->mode == CLIMATE_MODE_HEAT) ? 1 : 0;
          break;
        case CLIMATE_PRESET_SLEEP:
          out_data->quiet_mode = 0;
          out_data->fast_mode = 0;
          out_data->sleep_mode = 1;
          out_data->ten_degree = 0;
          break;
        default:
          ESP_LOGE("Control", "Unsupported preset");
          out_data->quiet_mode = 0;
          out_data->fast_mode = 0;
          out_data->sleep_mode = 0;
          out_data->ten_degree = 0;
          break;
      }
    }
  }
  if (this->pending_vertical_direction_.has_value()) {
    out_data->vertical_swing_mode = (uint8_t) this->pending_vertical_direction_.value();
    this->pending_vertical_direction_.reset();
  }
  if (this->pending_horizontal_direction_.has_value()) {
    out_data->horizontal_swing_mode = (uint8_t) this->pending_horizontal_direction_.value();
    this->pending_horizontal_direction_.reset();
  }
  out_data->beeper_status = ((!this->beeper_status_) || (!has_hvac_settings)) ? 1 : 0;
  control_out_buffer[4] = 0;  // This byte should be cleared before setting values
  out_data->display_status = this->display_status_ ? 1 : 0;
  out_data->health_mode = this->health_mode_ ? 1 : 0;
  return haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                      (uint16_t) hon_protocol::SubcommandsControl::SET_GROUP_PARAMETERS,
                                      control_out_buffer, sizeof(hon_protocol::HaierPacketControl));
}

void HonClimate::process_alarm_message_(const uint8_t *packet, uint8_t size, bool check_new) {
  constexpr size_t active_alarms_size = sizeof(this->active_alarms_);
  if (size >= active_alarms_size + 2) {
    if (check_new) {
      size_t alarm_code = 0;
      for (int i = active_alarms_size - 1; i >= 0; i--) {
        if (packet[2 + i] != active_alarms_[i]) {
          uint8_t alarm_bit = 1;
          for (int b = 0; b < 8; b++) {
            if ((packet[2 + i] & alarm_bit) != (this->active_alarms_[i] & alarm_bit)) {
              bool alarm_status = (packet[2 + i] & alarm_bit) != 0;
              int log_level = alarm_status ? ESPHOME_LOG_LEVEL_WARN : ESPHOME_LOG_LEVEL_INFO;
              const char *alarm_message = alarm_code < esphome::haier::hon_protocol::HON_ALARM_COUNT
                                              ? esphome::haier::hon_protocol::HON_ALARM_MESSAGES[alarm_code].c_str()
                                              : "Unknown";
              esp_log_printf_(log_level, TAG, __LINE__, "Alarm %s (%d): %s", alarm_status ? "activated" : "deactivated",
                              alarm_code, alarm_message);
              if (alarm_status) {
                this->alarm_start_callback_.call(alarm_code, alarm_message);
                this->active_alarm_count_ += 1.0f;
              } else {
                this->alarm_end_callback_.call(alarm_code, alarm_message);
                this->active_alarm_count_ -= 1.0f;
              }
            }
            alarm_bit <<= 1;
            alarm_code++;
          }
          active_alarms_[i] = packet[2 + i];
        } else
          alarm_code += 8;
      }
    } else {
      float alarm_count = 0.0f;
      static uint8_t nibble_bits_count[] = {0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4};
      for (size_t i = 0; i < sizeof(this->active_alarms_); i++) {
        alarm_count += (float) (nibble_bits_count[packet[2 + i] & 0x0F] + nibble_bits_count[packet[2 + i] >> 4]);
      }
      this->active_alarm_count_ = alarm_count;
      memcpy(this->active_alarms_, packet + 2, sizeof(this->active_alarms_));
    }
  }
}

#ifdef USE_SENSOR
void HonClimate::set_sub_sensor(SubSensorType type, sensor::Sensor *sens) {
  if (type < SubSensorType::SUB_SENSOR_TYPE_COUNT) {
    if (type >= SubSensorType::BIG_DATA_FRAME_SUB_SENSORS) {
      if ((this->sub_sensors_[(size_t) type] != nullptr) && (sens == nullptr)) {
        this->big_data_sensors_--;
      } else if ((this->sub_sensors_[(size_t) type] == nullptr) && (sens != nullptr)) {
        this->big_data_sensors_++;
      }
    }
    this->sub_sensors_[(size_t) type] = sens;
  }
}

void HonClimate::update_sub_sensor_(SubSensorType type, float value) {
  if (type < SubSensorType::SUB_SENSOR_TYPE_COUNT) {
    size_t index = (size_t) type;
    if ((this->sub_sensors_[index] != nullptr) &&
        ((!this->sub_sensors_[index]->has_state()) || (this->sub_sensors_[index]->raw_state != value)))
      this->sub_sensors_[index]->publish_state(value);
  }
}
#endif  // USE_SENSOR

#ifdef USE_BINARY_SENSOR
void HonClimate::set_sub_binary_sensor(SubBinarySensorType type, binary_sensor::BinarySensor *sens) {
  if (type < SubBinarySensorType::SUB_BINARY_SENSOR_TYPE_COUNT) {
    if ((this->sub_binary_sensors_[(size_t) type] != nullptr) && (sens == nullptr)) {
      this->big_data_sensors_--;
    } else if ((this->sub_binary_sensors_[(size_t) type] == nullptr) && (sens != nullptr)) {
      this->big_data_sensors_++;
    }
    this->sub_binary_sensors_[(size_t) type] = sens;
  }
}

void HonClimate::update_sub_binary_sensor_(SubBinarySensorType type, uint8_t value) {
  if (value < 2) {
    bool converted_value = value == 1;
    size_t index = (size_t) type;
    if ((this->sub_binary_sensors_[index] != nullptr) && ((!this->sub_binary_sensors_[index]->has_state()) ||
                                                          (this->sub_binary_sensors_[index]->state != converted_value)))
      this->sub_binary_sensors_[index]->publish_state(converted_value);
  }
}
#endif  // USE_BINARY_SENSOR

#ifdef USE_TEXT_SENSOR
void HonClimate::set_sub_text_sensor(SubTextSensorType type, text_sensor::TextSensor *sens) {
  this->sub_text_sensors_[(size_t) type] = sens;
  switch (type) {
    case SubTextSensorType::APPLIANCE_NAME:
      if (this->hvac_hardware_info_.has_value())
        sens->publish_state(this->hvac_hardware_info_.value().device_name_);
      break;
    case SubTextSensorType::PROTOCOL_VERSION:
      if (this->hvac_hardware_info_.has_value())
        sens->publish_state(this->hvac_hardware_info_.value().protocol_version_);
      break;
    case SubTextSensorType::CLEANING_STATUS:
      sens->publish_state(this->get_cleaning_status_text());
      break;
    default:
      break;
  }
}

void HonClimate::update_sub_text_sensor_(SubTextSensorType type, const std::string &value) {
  size_t index = (size_t) type;
  if (this->sub_text_sensors_[index] != nullptr)
    this->sub_text_sensors_[index]->publish_state(value);
}
#endif  // USE_TEXT_SENSOR

haier_protocol::HandlerError HonClimate::process_status_message_(const uint8_t *packet_buffer, uint8_t size) {
  size_t expected_size = 2 + sizeof(hon_protocol::HaierPacketControl) + sizeof(hon_protocol::HaierPacketSensors) +
                         this->extra_control_packet_bytes_;
  if (size < expected_size)
    return haier_protocol::HandlerError::WRONG_MESSAGE_STRUCTURE;
  uint16_t subtype = (((uint16_t) packet_buffer[0]) << 8) + packet_buffer[1];
  if ((subtype == 0x7D01) && (size >= expected_size + 4 + sizeof(hon_protocol::HaierPacketBigData))) {
    // Got BigData packet
    const hon_protocol::HaierPacketBigData *bd_packet =
        (const hon_protocol::HaierPacketBigData *) (&packet_buffer[expected_size + 4]);
#ifdef USE_SENSOR
    this->update_sub_sensor_(SubSensorType::INDOOR_COIL_TEMPERATURE, bd_packet->indoor_coil_temperature / 2.0 - 20);
    this->update_sub_sensor_(SubSensorType::OUTDOOR_COIL_TEMPERATURE, bd_packet->outdoor_coil_temperature - 64);
    this->update_sub_sensor_(SubSensorType::OUTDOOR_DEFROST_TEMPERATURE, bd_packet->outdoor_coil_temperature - 64);
    this->update_sub_sensor_(SubSensorType::OUTDOOR_IN_AIR_TEMPERATURE, bd_packet->outdoor_in_air_temperature - 64);
    this->update_sub_sensor_(SubSensorType::OUTDOOR_OUT_AIR_TEMPERATURE, bd_packet->outdoor_out_air_temperature - 64);
    this->update_sub_sensor_(SubSensorType::POWER, encode_uint16(bd_packet->power[0], bd_packet->power[1]));
    this->update_sub_sensor_(SubSensorType::COMPRESSOR_FREQUENCY, bd_packet->compressor_frequency);
    this->update_sub_sensor_(SubSensorType::COMPRESSOR_CURRENT,
                             encode_uint16(bd_packet->compressor_current[0], bd_packet->compressor_current[1]) / 10.0);
    this->update_sub_sensor_(
        SubSensorType::EXPANSION_VALVE_OPEN_DEGREE,
        encode_uint16(bd_packet->expansion_valve_open_degree[0], bd_packet->expansion_valve_open_degree[1]) / 4095.0);
#endif  // USE_SENSOR
#ifdef USE_BINARY_SENSOR
    this->update_sub_binary_sensor_(SubBinarySensorType::OUTDOOR_FAN_STATUS, bd_packet->outdoor_fan_status);
    this->update_sub_binary_sensor_(SubBinarySensorType::DEFROST_STATUS, bd_packet->defrost_status);
    this->update_sub_binary_sensor_(SubBinarySensorType::COMPRESSOR_STATUS, bd_packet->compressor_status);
    this->update_sub_binary_sensor_(SubBinarySensorType::INDOOR_FAN_STATUS, bd_packet->indoor_fan_status);
    this->update_sub_binary_sensor_(SubBinarySensorType::FOUR_WAY_VALVE_STATUS, bd_packet->four_way_valve_status);
    this->update_sub_binary_sensor_(SubBinarySensorType::INDOOR_ELECTRIC_HEATING_STATUS,
                                    bd_packet->indoor_electric_heating_status);
#endif  // USE_BINARY_SENSOR
  }
  struct {
    hon_protocol::HaierPacketControl control;
    hon_protocol::HaierPacketSensors sensors;
  } packet;
  memcpy(&packet.control, packet_buffer + 2, sizeof(hon_protocol::HaierPacketControl));
  memcpy(&packet.sensors,
         packet_buffer + 2 + sizeof(hon_protocol::HaierPacketControl) + this->extra_control_packet_bytes_,
         sizeof(hon_protocol::HaierPacketSensors));
  if (packet.sensors.error_status != 0) {
    ESP_LOGW(TAG, "HVAC error, code=0x%02X", packet.sensors.error_status);
  }
#ifdef USE_SENSOR
  if ((this->sub_sensors_[(size_t) SubSensorType::OUTDOOR_TEMPERATURE] != nullptr) &&
      (this->got_valid_outdoor_temp_ || (packet.sensors.outdoor_temperature > 0))) {
    this->got_valid_outdoor_temp_ = true;
    this->update_sub_sensor_(SubSensorType::OUTDOOR_TEMPERATURE,
                             (float) (packet.sensors.outdoor_temperature + PROTOCOL_OUTDOOR_TEMPERATURE_OFFSET));
  }
  if ((this->sub_sensors_[(size_t) SubSensorType::HUMIDITY] != nullptr) && (packet.sensors.room_humidity <= 100)) {
    this->update_sub_sensor_(SubSensorType::HUMIDITY, (float) packet.sensors.room_humidity);
  }
#endif  // USE_SENSOR
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
    } else if (packet.control.ten_degree != 0) {
      this->preset = CLIMATE_PRESET_AWAY;
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
          this->force_send_control_ = true;
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
        this->action_request_ =
            PendingAction({ActionRequest::TURN_POWER_OFF, esphome::optional<haier_protocol::HaierMessage>()});
      }
      this->cleaning_status_ = new_cleaning;
#ifdef USE_TEXT_SENSOR
      this->update_sub_text_sensor_(SubTextSensorType::CLEANING_STATUS, this->get_cleaning_status_text());
#endif  // USE_TEXT_SENSOR
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
    // Saving last known non auto mode for vertical and horizontal swing
    this->current_vertical_swing_ = (hon_protocol::VerticalSwingMode) packet.control.vertical_swing_mode;
    this->current_horizontal_swing_ = (hon_protocol::HorizontalSwingMode) packet.control.horizontal_swing_mode;
    bool save_settings = ((this->current_vertical_swing_.value() != hon_protocol::VerticalSwingMode::AUTO) &&
                          (this->current_vertical_swing_.value() != hon_protocol::VerticalSwingMode::AUTO_SPECIAL) &&
                          (this->current_vertical_swing_.value() != this->settings_.last_vertiacal_swing)) ||
                         ((this->current_horizontal_swing_.value() != hon_protocol::HorizontalSwingMode::AUTO) &&
                          (this->current_horizontal_swing_.value() != this->settings_.last_horizontal_swing));
    if (save_settings) {
      this->settings_.last_vertiacal_swing = this->current_vertical_swing_.value();
      this->settings_.last_horizontal_swing = this->current_horizontal_swing_.value();
      this->rtc_.save(&this->settings_);
    }
    should_publish = should_publish || (old_swing_mode != this->swing_mode);
  }
  this->last_valid_status_timestamp_ = std::chrono::steady_clock::now();
  if (should_publish) {
    this->publish_state();
  }
  if (should_publish) {
    ESP_LOGI(TAG, "HVAC values changed");
  }
  int log_level = should_publish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG;
  esp_log_printf_(log_level, TAG, __LINE__, "HVAC Mode = 0x%X", packet.control.ac_mode);
  esp_log_printf_(log_level, TAG, __LINE__, "Fan speed Status = 0x%X", packet.control.fan_mode);
  esp_log_printf_(log_level, TAG, __LINE__, "Horizontal Swing Status = 0x%X", packet.control.horizontal_swing_mode);
  esp_log_printf_(log_level, TAG, __LINE__, "Vertical Swing Status = 0x%X", packet.control.vertical_swing_mode);
  esp_log_printf_(log_level, TAG, __LINE__, "Set Point Status = 0x%X", packet.control.set_point);
  return haier_protocol::HandlerError::HANDLER_OK;
}

void HonClimate::fill_control_messages_queue_() {
  static uint8_t one_buf[] = {0x00, 0x01};
  static uint8_t zero_buf[] = {0x00, 0x00};
  if (!this->current_hvac_settings_.valid && !this->force_send_control_)
    return;
  this->clear_control_messages_queue_();
  HvacSettings climate_control;
  climate_control = this->current_hvac_settings_;
  // Beeper command
  {
    this->control_messages_queue_.push(
        haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                     (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                         (uint8_t) hon_protocol::DataParameters::BEEPER_STATUS,
                                     this->beeper_status_ ? zero_buf : one_buf, 2));
  }
  // Health mode
  {
    this->control_messages_queue_.push(
        haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                     (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                         (uint8_t) hon_protocol::DataParameters::HEALTH_MODE,
                                     this->health_mode_ ? one_buf : zero_buf, 2));
  }
  // Climate mode
  bool new_power = this->mode != CLIMATE_MODE_OFF;
  uint8_t fan_mode_buf[] = {0x00, 0xFF};
  uint8_t quiet_mode_buf[] = {0x00, 0xFF};
  if (climate_control.mode.has_value()) {
    uint8_t buffer[2] = {0x00, 0x00};
    switch (climate_control.mode.value()) {
      case CLIMATE_MODE_OFF:
        new_power = false;
        break;
      case CLIMATE_MODE_HEAT_COOL:
        new_power = true;
        buffer[1] = (uint8_t) hon_protocol::ConditioningMode::AUTO;
        this->control_messages_queue_.push(
            haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                         (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                             (uint8_t) hon_protocol::DataParameters::AC_MODE,
                                         buffer, 2));
        fan_mode_buf[1] = this->other_modes_fan_speed_;
        break;
      case CLIMATE_MODE_HEAT:
        new_power = true;
        buffer[1] = (uint8_t) hon_protocol::ConditioningMode::HEAT;
        this->control_messages_queue_.push(
            haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                         (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                             (uint8_t) hon_protocol::DataParameters::AC_MODE,
                                         buffer, 2));
        fan_mode_buf[1] = this->other_modes_fan_speed_;
        break;
      case CLIMATE_MODE_DRY:
        new_power = true;
        buffer[1] = (uint8_t) hon_protocol::ConditioningMode::DRY;
        this->control_messages_queue_.push(
            haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                         (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                             (uint8_t) hon_protocol::DataParameters::AC_MODE,
                                         buffer, 2));
        fan_mode_buf[1] = this->other_modes_fan_speed_;
        break;
      case CLIMATE_MODE_FAN_ONLY:
        new_power = true;
        buffer[1] = (uint8_t) hon_protocol::ConditioningMode::FAN;
        this->control_messages_queue_.push(
            haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                         (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                             (uint8_t) hon_protocol::DataParameters::AC_MODE,
                                         buffer, 2));
        fan_mode_buf[1] = this->other_modes_fan_speed_;  // Auto doesn't work in fan only mode
        // Disabling eco mode for Fan only
        quiet_mode_buf[1] = 0;
        break;
      case CLIMATE_MODE_COOL:
        new_power = true;
        buffer[1] = (uint8_t) hon_protocol::ConditioningMode::COOL;
        this->control_messages_queue_.push(
            haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                         (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                             (uint8_t) hon_protocol::DataParameters::AC_MODE,
                                         buffer, 2));
        fan_mode_buf[1] = this->other_modes_fan_speed_;
        break;
      default:
        ESP_LOGE("Control", "Unsupported climate mode");
        break;
    }
  }
  // Climate power
  {
    this->control_messages_queue_.push(
        haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                     (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                         (uint8_t) hon_protocol::DataParameters::AC_POWER,
                                     new_power ? one_buf : zero_buf, 2));
  }
  // CLimate preset
  {
    uint8_t fast_mode_buf[] = {0x00, 0xFF};
    uint8_t away_mode_buf[] = {0x00, 0xFF};
    if (!new_power) {
      // If AC is off - no presets allowed
      quiet_mode_buf[1] = 0x00;
      fast_mode_buf[1] = 0x00;
      away_mode_buf[1] = 0x00;
    } else if (climate_control.preset.has_value()) {
      switch (climate_control.preset.value()) {
        case CLIMATE_PRESET_NONE:
          quiet_mode_buf[1] = 0x00;
          fast_mode_buf[1] = 0x00;
          away_mode_buf[1] = 0x00;
          break;
        case CLIMATE_PRESET_ECO:
          // Eco is not supported in Fan only mode
          quiet_mode_buf[1] = (this->mode != CLIMATE_MODE_FAN_ONLY) ? 0x01 : 0x00;
          fast_mode_buf[1] = 0x00;
          away_mode_buf[1] = 0x00;
          break;
        case CLIMATE_PRESET_BOOST:
          quiet_mode_buf[1] = 0x00;
          // Boost is not supported in Fan only mode
          fast_mode_buf[1] = (this->mode != CLIMATE_MODE_FAN_ONLY) ? 0x01 : 0x00;
          away_mode_buf[1] = 0x00;
          break;
        case CLIMATE_PRESET_AWAY:
          quiet_mode_buf[1] = 0x00;
          fast_mode_buf[1] = 0x00;
          away_mode_buf[1] = (this->mode == CLIMATE_MODE_HEAT) ? 0x01 : 0x00;
          break;
        default:
          ESP_LOGE("Control", "Unsupported preset");
          break;
      }
    }
    auto presets = this->traits_.get_supported_presets();
    if ((quiet_mode_buf[1] != 0xFF) && ((presets.find(climate::ClimatePreset::CLIMATE_PRESET_ECO) != presets.end()))) {
      this->control_messages_queue_.push(
          haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                       (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                           (uint8_t) hon_protocol::DataParameters::QUIET_MODE,
                                       quiet_mode_buf, 2));
    }
    if ((fast_mode_buf[1] != 0xFF) && ((presets.find(climate::ClimatePreset::CLIMATE_PRESET_BOOST) != presets.end()))) {
      this->control_messages_queue_.push(
          haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                       (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                           (uint8_t) hon_protocol::DataParameters::FAST_MODE,
                                       fast_mode_buf, 2));
    }
    if ((away_mode_buf[1] != 0xFF) && ((presets.find(climate::ClimatePreset::CLIMATE_PRESET_AWAY) != presets.end()))) {
      this->control_messages_queue_.push(
          haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                       (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                           (uint8_t) hon_protocol::DataParameters::TEN_DEGREE,
                                       away_mode_buf, 2));
    }
  }
  // Target temperature
  if (climate_control.target_temperature.has_value() && (this->mode != ClimateMode::CLIMATE_MODE_FAN_ONLY)) {
    uint8_t buffer[2] = {0x00, 0x00};
    buffer[1] = ((uint8_t) climate_control.target_temperature.value()) - 16;
    this->control_messages_queue_.push(
        haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                     (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                         (uint8_t) hon_protocol::DataParameters::SET_POINT,
                                     buffer, 2));
  }
  // Fan mode
  if (climate_control.fan_mode.has_value()) {
    switch (climate_control.fan_mode.value()) {
      case CLIMATE_FAN_LOW:
        fan_mode_buf[1] = (uint8_t) hon_protocol::FanMode::FAN_LOW;
        break;
      case CLIMATE_FAN_MEDIUM:
        fan_mode_buf[1] = (uint8_t) hon_protocol::FanMode::FAN_MID;
        break;
      case CLIMATE_FAN_HIGH:
        fan_mode_buf[1] = (uint8_t) hon_protocol::FanMode::FAN_HIGH;
        break;
      case CLIMATE_FAN_AUTO:
        if (mode != CLIMATE_MODE_FAN_ONLY)  // if we are not in fan only mode
          fan_mode_buf[1] = (uint8_t) hon_protocol::FanMode::FAN_AUTO;
        break;
      default:
        ESP_LOGE("Control", "Unsupported fan mode");
        break;
    }
    if (fan_mode_buf[1] != 0xFF) {
      this->control_messages_queue_.push(
          haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL,
                                       (uint16_t) hon_protocol::SubcommandsControl::SET_SINGLE_PARAMETER +
                                           (uint8_t) hon_protocol::DataParameters::FAN_MODE,
                                       fan_mode_buf, 2));
    }
  }
}

void HonClimate::clear_control_messages_queue_() {
  while (!this->control_messages_queue_.empty())
    this->control_messages_queue_.pop();
}

bool HonClimate::prepare_pending_action() {
  switch (this->action_request_.value().action) {
    case ActionRequest::START_SELF_CLEAN: {
      uint8_t control_out_buffer[sizeof(hon_protocol::HaierPacketControl)];
      memcpy(control_out_buffer, this->last_status_message_.get(), sizeof(hon_protocol::HaierPacketControl));
      hon_protocol::HaierPacketControl *out_data = (hon_protocol::HaierPacketControl *) control_out_buffer;
      out_data->self_cleaning_status = 1;
      out_data->steri_clean = 0;
      out_data->set_point = 0x06;
      out_data->vertical_swing_mode = (uint8_t) hon_protocol::VerticalSwingMode::CENTER;
      out_data->horizontal_swing_mode = (uint8_t) hon_protocol::HorizontalSwingMode::CENTER;
      out_data->ac_power = 1;
      out_data->ac_mode = (uint8_t) hon_protocol::ConditioningMode::DRY;
      out_data->light_status = 0;
      this->action_request_.value().message = haier_protocol::HaierMessage(
          haier_protocol::FrameType::CONTROL, (uint16_t) hon_protocol::SubcommandsControl::SET_GROUP_PARAMETERS,
          control_out_buffer, sizeof(hon_protocol::HaierPacketControl));
    }
      return true;
    case ActionRequest::START_STERI_CLEAN: {
      uint8_t control_out_buffer[sizeof(hon_protocol::HaierPacketControl)];
      memcpy(control_out_buffer, this->last_status_message_.get(), sizeof(hon_protocol::HaierPacketControl));
      hon_protocol::HaierPacketControl *out_data = (hon_protocol::HaierPacketControl *) control_out_buffer;
      out_data->self_cleaning_status = 0;
      out_data->steri_clean = 1;
      out_data->set_point = 0x06;
      out_data->vertical_swing_mode = (uint8_t) hon_protocol::VerticalSwingMode::CENTER;
      out_data->horizontal_swing_mode = (uint8_t) hon_protocol::HorizontalSwingMode::CENTER;
      out_data->ac_power = 1;
      out_data->ac_mode = (uint8_t) hon_protocol::ConditioningMode::DRY;
      out_data->light_status = 0;
      this->action_request_.value().message = haier_protocol::HaierMessage(
          haier_protocol::FrameType::CONTROL, (uint16_t) hon_protocol::SubcommandsControl::SET_GROUP_PARAMETERS,
          control_out_buffer, sizeof(hon_protocol::HaierPacketControl));
    }
      return true;
    default:
      return HaierClimateBase::prepare_pending_action();
  }
}

void HonClimate::process_protocol_reset() {
  HaierClimateBase::process_protocol_reset();
#ifdef USE_SENSOR
  for (auto &sub_sensor : this->sub_sensors_) {
    if ((sub_sensor != nullptr) && sub_sensor->has_state())
      sub_sensor->publish_state(NAN);
  }
#endif  // USE_SENSOR
  this->got_valid_outdoor_temp_ = false;
  this->hvac_hardware_info_.reset();
}

bool HonClimate::should_get_big_data_() {
  if (this->big_data_sensors_ > 0) {
    static uint8_t counter = 0;
    counter = (counter + 1) % 3;
    return counter == 1;
  }
  return false;
}

}  // namespace haier
}  // namespace esphome
