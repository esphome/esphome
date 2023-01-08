#include "esphome.h"
#include <chrono>
#include <string>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#ifdef HAIER_REPORT_WIFI_SIGNAL
#include "esphome/components/wifi/wifi_component.h"
#endif
#include "haier_climate.h"
#include "haier_packet.h"
#include "protocol/haier_protocol.h"

using namespace esphome::climate;
using namespace esphome::uart;


#ifndef ESPHOME_LOG_LEVEL
#warning "No ESPHOME_LOG_LEVEL defined!"
#endif

namespace esphome {
namespace haier {

const char TAG[] = "haier.climate";
constexpr size_t COMMUNICATION_TIMEOUT_MS =         60000;
constexpr size_t STATUS_REQUEST_INTERVAL_MS =       5000;
constexpr size_t DEFAULT_MESSAGES_INTERVAL_MS =     2000;
constexpr size_t CONTROL_MESSAGES_INTERVAL_MS =     400;
#ifdef HAIER_REPORT_WIFI_SIGNAL
constexpr size_t SIGNAL_LEVEL_UPDATE_INTERVAL_MS =  10000;
#endif
constexpr size_t CONTROL_TIMEOUT_MS =               7000;
constexpr int PROTOCOL_OUTDOOR_TEMPERATURE_OFFSET = -64;

#if (HAIER_LOG_LEVEL > 4)
// To reduce size of binary this function only available when log level is Verbose
const char* HaierClimate::phase_to_string_(ProtocolPhases phase)
{
  static const char* phase_names[] = {
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
    "UNKNOWN"     // Should be the last!
  };
  int phase_index = (int)phase;
  if (phase_index > (int)ProtocolPhases::NUM_PROTOCOL_PHASES)
    phase_index = (int)ProtocolPhases::NUM_PROTOCOL_PHASES;
  return phase_names[phase_index];
}
#endif

hon_protocol::VerticalSwingMode get_vertical_swing_mode(AirflowVerticalDirection direction)
{
  switch (direction)
  {
    case AirflowVerticalDirection::UP:
      return hon_protocol::VerticalSwingMode::UP;
    case AirflowVerticalDirection::DOWN:
      return hon_protocol::VerticalSwingMode::DOWN;
    default:
      return hon_protocol::VerticalSwingMode::CENTER;
  }
}

hon_protocol::HorizontalSwingMode get_horizontal_swing_mode(AirflowHorizontalDirection direction)
{
  switch (direction)
  {
    case AirflowHorizontalDirection::LEFT:
      return hon_protocol::HorizontalSwingMode::LEFT;
    case AirflowHorizontalDirection::RIGHT:
      return hon_protocol::HorizontalSwingMode::RIGHT;
    default:
      return hon_protocol::HorizontalSwingMode::CENTER;
  }
}

HaierClimate::HaierClimate(UARTComponent* parent) :
              Component(),
              UARTDevice(parent),
              haier_protocol_(*this),
              protocol_phase_(ProtocolPhases::SENDING_INIT_1),
              fan_mode_speed_((uint8_t)hon_protocol::FanMode::FAN_MID),
              other_modes_fan_speed_((uint8_t)hon_protocol::FanMode::FAN_AUTO),
              beeper_status_(true),
              display_status_(true),
              force_send_control_(false),
              forced_publish_(false),
              forced_request_status_(false),
              control_called_(false),
              got_valid_outdoor_temp_(false),
              hvac_hardware_info_available_(false),
              hvac_functions_{false, false, false, false, false},
              use_crc_(hvac_functions_[2]),
              active_alarms_{0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
              outdoor_sensor_(nullptr)
{
  this->last_status_message_ = new uint8_t[sizeof(hon_protocol::HaierPacketControl)];
  this->traits_ = climate::ClimateTraits();
  this->traits_.set_supported_modes(
  {
    climate::CLIMATE_MODE_OFF,
    climate::CLIMATE_MODE_COOL,
    climate::CLIMATE_MODE_HEAT,
    climate::CLIMATE_MODE_FAN_ONLY,
    climate::CLIMATE_MODE_DRY,
    climate::CLIMATE_MODE_AUTO
  });
  this->traits_.set_supported_fan_modes(
  {
    climate::CLIMATE_FAN_AUTO,
    climate::CLIMATE_FAN_LOW,
    climate::CLIMATE_FAN_MEDIUM,
    climate::CLIMATE_FAN_HIGH,
  });
  this->traits_.set_supported_swing_modes(
  {
    climate::CLIMATE_SWING_OFF,
    climate::CLIMATE_SWING_BOTH,
    climate::CLIMATE_SWING_VERTICAL,
    climate::CLIMATE_SWING_HORIZONTAL
  });
  this->traits_.set_supported_presets(
  {
    climate::CLIMATE_PRESET_NONE,
    climate::CLIMATE_PRESET_ECO,
    climate::CLIMATE_PRESET_BOOST,
    climate::CLIMATE_PRESET_SLEEP,
  });
  this->traits_.set_supports_current_temperature(true);
  this->vertical_direction_ = AirflowVerticalDirection::CENTER;
  this->horizontal_direction_ = AirflowHorizontalDirection::CENTER;
 }

HaierClimate::~HaierClimate()
{
  delete[] this->last_status_message_;
}

void HaierClimate::set_phase(ProtocolPhases phase)
{
  if (this->protocol_phase_ != phase)
  {
    ESP_LOGV(TAG, "Phase transition: %s => %s", phase_to_string_(this->protocol_phase_), phase_to_string_(phase));
    this->protocol_phase_ = phase;
  }
}

void HaierClimate::set_beeper_state(bool state)
{
  this->beeper_status_ = state;
}

bool HaierClimate::get_beeper_state() const
{
  return this->beeper_status_;
}

bool HaierClimate::get_display_state() const
{
  return this->display_status_;
}

void HaierClimate::set_display_state(bool state)
{
  if (this->display_status_ != state)
  {
    this->display_status_ = state;
    this->force_send_control_ = true;
  }
}

void HaierClimate::set_outdoor_temperature_sensor(esphome::sensor::Sensor *sensor) 
{
  this->outdoor_sensor_ = sensor; 
}

AirflowVerticalDirection HaierClimate::get_vertical_airflow() const
{
  return this->vertical_direction_;
};

void HaierClimate::set_vertical_airflow(AirflowVerticalDirection direction)
{
  if (direction > AirflowVerticalDirection::DOWN)
    this->vertical_direction_ = AirflowVerticalDirection::CENTER;
  else
    this->vertical_direction_ = direction;
  this->force_send_control_ = true;
}

AirflowHorizontalDirection HaierClimate::get_horizontal_airflow() const
{
  return this->horizontal_direction_;
}

void HaierClimate::set_horizontal_airflow(AirflowHorizontalDirection direction)
{
  if (direction > AirflowHorizontalDirection::RIGHT)
    this->horizontal_direction_ = AirflowHorizontalDirection::CENTER;
  else
    this->horizontal_direction_ = direction;
  this->force_send_control_ = true;
}

void HaierClimate::set_supported_swing_modes(const std::set<climate::ClimateSwingMode> &modes)
{
  this->traits_.set_supported_swing_modes(modes);
  this->traits_.add_supported_swing_mode(climate::CLIMATE_SWING_OFF);   // Always available
  this->traits_.add_supported_swing_mode(CLIMATE_SWING_VERTICAL);       // Always available
}


haier_protocol::HandlerError HaierClimate::answer_preprocess(uint8_t requestMessageType, uint8_t expectedRequestMessageType, uint8_t answerMessageType, uint8_t expectedAnswerMessageType, ProtocolPhases expectedPhase)
{
  haier_protocol::HandlerError result = haier_protocol::HandlerError::HANDLER_OK;
  if ((expectedRequestMessageType != (uint8_t)hon_protocol::FrameType::NO_COMMAND) && (requestMessageType != expectedRequestMessageType))
    result = haier_protocol::HandlerError::UNSUPORTED_MESSAGE;
  if ((expectedAnswerMessageType != (uint8_t)hon_protocol::FrameType::NO_COMMAND) && (answerMessageType != expectedAnswerMessageType))
    result = haier_protocol::HandlerError::UNSUPORTED_MESSAGE;
  if ((expectedPhase != ProtocolPhases::UNKNOWN) && (expectedPhase != this->protocol_phase_))
    result = haier_protocol::HandlerError::UNEXPECTED_MESSAGE;
  if (answerMessageType == (uint8_t)hon_protocol::FrameType::INVALID)
    result = haier_protocol::HandlerError::INVALID_ANSWER;
  return result;
}

haier_protocol::HandlerError HaierClimate::get_device_version_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  haier_protocol::HandlerError result = this->answer_preprocess(requestType, (uint8_t)hon_protocol::FrameType::GET_DEVICE_VERSION, messageType, (uint8_t)hon_protocol::FrameType::GET_DEVICE_VERSION_RESPONSE, ProtocolPhases::WAITING_ANSWER_INIT_1);
  if (result == haier_protocol::HandlerError::HANDLER_OK)
  {
    if (dataSize < sizeof(hon_protocol::DeviceVersionAnswer))
    {
      // Wrong structure
      this->set_phase(ProtocolPhases::SENDING_INIT_1);
      return haier_protocol::HandlerError::WRONG_MESSAGE_STRUCTURE;
    }
    // All OK
    hon_protocol::DeviceVersionAnswer* answr = (hon_protocol::DeviceVersionAnswer*)data;
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
    this->hvac_functions_[0] = (answr->functions[1] & 0x01) != 0;      // interactive mode support
    this->hvac_functions_[1] = (answr->functions[1] & 0x02) != 0;      // master-slave mode support
    this->hvac_functions_[2] = (answr->functions[1] & 0x04) != 0;      // crc support
    this->hvac_functions_[3] = (answr->functions[1] & 0x08) != 0;      // multiple AC support
    this->hvac_functions_[4] = (answr->functions[1] & 0x20) != 0;      // roles support
    this->hvac_hardware_info_available_ = true;
    this->set_phase(ProtocolPhases::SENDING_INIT_2);
    return result;
  }
  else
  {
    this->set_phase((this->protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

haier_protocol::HandlerError HaierClimate::get_device_id_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  haier_protocol::HandlerError result = this->answer_preprocess(requestType, (uint8_t)hon_protocol::FrameType::GET_DEVICE_ID, messageType, (uint8_t)hon_protocol::FrameType::GET_DEVICE_ID_RESPONSE, ProtocolPhases::WAITING_ANSWER_INIT_2);
  if (result == haier_protocol::HandlerError::HANDLER_OK)
  {
    this->set_phase(ProtocolPhases::SENDING_FIRST_STATUS_REQUEST);
    return result;
  }
  else
  {
    this->set_phase((this->protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

haier_protocol::HandlerError HaierClimate::status_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  haier_protocol::HandlerError result = this->answer_preprocess(requestType, (uint8_t)hon_protocol::FrameType::CONTROL, messageType, (uint8_t)hon_protocol::FrameType::STATUS, ProtocolPhases::UNKNOWN);
  if (result == haier_protocol::HandlerError::HANDLER_OK)
  {
    result = this->process_status_message(data, dataSize);
    if (result != haier_protocol::HandlerError::HANDLER_OK)
    {
      ESP_LOGW(TAG, "Error %d while parsing Status packet", (int)result);
      this->set_phase((this->protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    }
    else
    {
      if (dataSize >= sizeof(hon_protocol::HaierPacketControl) + 2)
      {
        memcpy(this->last_status_message_, data + 2, sizeof(hon_protocol::HaierPacketControl));
      }
      else
        ESP_LOGW(TAG, "Status packet too small: %d (should be >= %d)", dataSize, sizeof(hon_protocol::HaierPacketControl));
      if (this->protocol_phase_ == ProtocolPhases::WAITING_FIRST_STATUS_ANSWER)
      {
        ESP_LOGI(TAG, "First HVAC status received");
        this->set_phase(ProtocolPhases::SENDING_ALARM_STATUS_REQUEST);
      }
      else if (this->protocol_phase_ == ProtocolPhases::WAITING_STATUS_ANSWER)
        this->set_phase(ProtocolPhases::IDLE);
      else if (this->protocol_phase_ == ProtocolPhases::WAITING_CONTROL_ANSWER)
      {
        this->set_phase(ProtocolPhases::IDLE); 
        this->force_send_control_ = false;
        if (this->hvac_settings_.valid)
          this->hvac_settings_.reset();
      }
    }
    return result;
  }
  else
  {
    this->set_phase((this->protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE : ProtocolPhases::SENDING_INIT_1);
    return result;
  }
}

haier_protocol::HandlerError HaierClimate::get_management_information_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  haier_protocol::HandlerError result = this->answer_preprocess(requestType, (uint8_t)hon_protocol::FrameType::GET_MANAGEMENT_INFORMATION, messageType, (uint8_t)hon_protocol::FrameType::GET_MANAGEMENT_INFORMATION_RESPONSE, ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER);
  if (result == haier_protocol::HandlerError::HANDLER_OK)
  {
    this->set_phase(ProtocolPhases::SENDING_SIGNAL_LEVEL);
    return result;
  }
  else
  {
    this->set_phase(ProtocolPhases::IDLE);
    return result;
  }
}

haier_protocol::HandlerError HaierClimate::report_network_status_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  haier_protocol::HandlerError result = this->answer_preprocess(requestType, (uint8_t)hon_protocol::FrameType::REPORT_NETWORK_STATUS, messageType, (uint8_t)hon_protocol::FrameType::CONFIRM, ProtocolPhases::WAITING_SIGNAL_LEVEL_ANSWER);
  if (result == haier_protocol::HandlerError::HANDLER_OK)
  {
    this->set_phase(ProtocolPhases::IDLE);
    return result;
  }
  else
  {
    this->set_phase(ProtocolPhases::IDLE);
    return result;
  }
}

haier_protocol::HandlerError HaierClimate::get_alarm_status_answer_handler(uint8_t requestType, uint8_t messageType, const uint8_t* data, size_t dataSize)
{
  if (requestType == (uint8_t)hon_protocol::FrameType::GET_ALARM_STATUS)
  {
    if (messageType != (uint8_t)hon_protocol::FrameType::GET_ALARM_STATUS_RESPONSE)
    {
      // Unexpected answer to request
      this->set_phase(ProtocolPhases::IDLE);
      return haier_protocol::HandlerError::UNSUPORTED_MESSAGE;
    }
    if (this->protocol_phase_ != ProtocolPhases::WAITING_ALARM_STATUS_ANSWER)
    {
      // Don't expect this answer now
      this->set_phase(ProtocolPhases::IDLE);
      return haier_protocol::HandlerError::UNEXPECTED_MESSAGE;
    }
    memcpy(this->active_alarms_, data + 2, 8);
    this->set_phase(ProtocolPhases::IDLE);        return haier_protocol::HandlerError::HANDLER_OK;
  }
  else
  {
    this->set_phase(ProtocolPhases::IDLE);
    return haier_protocol::HandlerError::UNSUPORTED_MESSAGE;
  }
}

haier_protocol::HandlerError HaierClimate::timeout_default_handler(uint8_t requestType)
{
#if (HAIER_LOG_LEVEL > 4)
  ESP_LOGW(TAG, "Answer timeout for command %02X, phase %s", requestType, phase_to_string_(this->protocol_phase_));
#else
  ESP_LOGW(TAG, "Answer timeout for command %02X, phase %d", requestType, this->protocol_phase_);
#endif
  if (this->protocol_phase_ > ProtocolPhases::IDLE)
    this->set_phase(ProtocolPhases::IDLE);
  else
    this->set_phase(ProtocolPhases::SENDING_INIT_1);
  return haier_protocol::HandlerError::HANDLER_OK;
}

void HaierClimate::setup()
{
  ESP_LOGI(TAG, "Haier initialization...");
  // Set timestamp here to give AC time to boot
  this->last_request_timestamp_ = std::chrono::steady_clock::now();
  this->set_phase(ProtocolPhases::SENDING_INIT_1);
  // Set handlers
#define SET_HANDLER(command, handler) this->haier_protocol_.set_answer_handler((uint8_t)(command), std::bind(&handler, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  SET_HANDLER(hon_protocol::FrameType::GET_DEVICE_VERSION, esphome::haier::HaierClimate::get_device_version_answer_handler);
  SET_HANDLER(hon_protocol::FrameType::GET_DEVICE_ID, esphome::haier::HaierClimate::get_device_id_answer_handler);
  SET_HANDLER(hon_protocol::FrameType::CONTROL, esphome::haier::HaierClimate::status_handler);
  SET_HANDLER(hon_protocol::FrameType::GET_MANAGEMENT_INFORMATION, esphome::haier::HaierClimate::get_management_information_answer_handler);
  SET_HANDLER(hon_protocol::FrameType::GET_ALARM_STATUS, esphome::haier::HaierClimate::get_alarm_status_answer_handler);
  SET_HANDLER(hon_protocol::FrameType::REPORT_NETWORK_STATUS, esphome::haier::HaierClimate::report_network_status_answer_handler);
#undef SET_HANDLER
  this->haier_protocol_.set_default_timeout_handler(std::bind(&esphome::haier::HaierClimate::timeout_default_handler, this, std::placeholders::_1));
}

void HaierClimate::dump_config()
{
  LOG_CLIMATE("", "Haier hOn Climate", this);
  if (this->hvac_hardware_info_available_)
  {
    ESP_LOGCONFIG(TAG, "  Device protocol version: %s",
      this->hvac_protocol_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device software version: %s", 
      this->hvac_software_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device hardware version: %s",
      this->hvac_hardware_version_.c_str());
    ESP_LOGCONFIG(TAG, "  Device name: %s", 
      this->hvac_device_name_.c_str());
    ESP_LOGCONFIG(TAG, "  Device features:%s%s%s%s%s",
      (this->hvac_functions_[0] ? " interactive" : ""), 
      (this->hvac_functions_[1] ? " master-slave" : ""),
      (this->hvac_functions_[2] ? " crc" : ""), 
      (this->hvac_functions_[3] ? " multinode" : ""),
      (this->hvac_functions_[4] ? " role" : ""));
    ESP_LOGCONFIG(TAG, "  Active alarms: %s", 
      buf_to_hex(this->active_alarms_, sizeof(this->active_alarms_)).c_str());
  }
}

void HaierClimate::loop()
{
  std::chrono::steady_clock::time_point now = std::chrono::steady_clock::now();
  if (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_valid_status_timestamp_).count() > COMMUNICATION_TIMEOUT_MS)
  {
    if (this->protocol_phase_ > ProtocolPhases::IDLE)
    {
      // No status too long, reseting protocol
      ESP_LOGW(TAG, "Communication timeout, reseting protocol");
      this->last_valid_status_timestamp_ = now;
      this->force_send_control_ = false;
      if (this->hvac_settings_.valid)
        this->hvac_settings_.reset();
      this->set_phase(ProtocolPhases::SENDING_INIT_1);
      return;
    }
    else
      // No need to reset protocol if we didn't pass initialization phase
      this->last_valid_status_timestamp_ = now;
  };
  if (this->hvac_settings_.valid || this->force_send_control_)
  {
    // If control message is pending we should send it ASAP unless we are in initialisation procedure or waiting for an answer
    if ((this->protocol_phase_ == ProtocolPhases::IDLE) ||
        (this->protocol_phase_ == ProtocolPhases::SENDING_STATUS_REQUEST) || 
        (this->protocol_phase_ == ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST) ||
        (this->protocol_phase_ == ProtocolPhases::SENDING_SIGNAL_LEVEL))
    {
      ESP_LOGV(TAG, "Control packet is pending...");
      this->control_request_timestamp_ = now;
      this->set_phase(ProtocolPhases::SENDING_CONTROL);
    }
  }
  switch (this->protocol_phase_)
  {
    case ProtocolPhases::SENDING_INIT_1:
      if (this->can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        this->hvac_hardware_info_available_ = false;
        // Indicate device capabilities:
        // bit 0 - if 1 module support interactive mode
        // bit 1 - if 1 module support master-slave mode
        // bit 2 - if 1 module support crc
        // bit 3 - if 1 module support multiple devices
        // bit 4..bit 15 - not used
        uint8_t module_capabilities[2] = { 0b00000000, 0b00000111 };     
        static const haier_protocol::HaierMessage device_version_request((uint8_t)hon_protocol::FrameType::GET_DEVICE_VERSION, module_capabilities, sizeof(module_capabilities));
        this->send_message(device_version_request);
        this->set_phase(ProtocolPhases::WAITING_ANSWER_INIT_1);
      }
      break;
    case ProtocolPhases::SENDING_INIT_2:
      if (this->can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const haier_protocol::HaierMessage deviceId_request((uint8_t)hon_protocol::FrameType::GET_DEVICE_ID);
        this->send_message(deviceId_request);
        this->set_phase(ProtocolPhases::WAITING_ANSWER_INIT_2);
      }
      break;
    case ProtocolPhases::SENDING_FIRST_STATUS_REQUEST:
    case ProtocolPhases::SENDING_STATUS_REQUEST:
      if (this->can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const haier_protocol::HaierMessage status_request((uint8_t)hon_protocol::FrameType::CONTROL, (uint16_t)hon_protocol::SubcomandsControl::GET_USER_DATA);
        this->send_message(status_request);
        this->last_status_request_ = now;
        this->set_phase((ProtocolPhases)((uint8_t)this->protocol_phase_ + 1));
      }
      break;
#ifdef HAIER_REPORT_WIFI_SIGNAL
    case ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST:
      if (this->can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const haier_protocol::HaierMessage update_signal_request((uint8_t)hon_protocol::FrameType::GET_MANAGEMENT_INFORMATION);
        this->send_message(update_signal_request);
        this->last_signal_request_ = now;
        this->set_phase(ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_SIGNAL_LEVEL:
      if (this->can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static uint8_t wifi_status_data[4] = { 0x00, 0x00, 0x00, 0x00 };
        if (wifi::global_wifi_component->is_connected())
        {
          wifi_status_data[1] = 0;
          int8_t _rssi = wifi::global_wifi_component->wifi_rssi();
          wifi_status_data[3] = uint8_t((128 + _rssi) / 1.28f);
          ESP_LOGD(TAG, "WiFi signal is: %ddBm => %d%%", _rssi, wifi_status_data[3]);
        }
        else
        {
          ESP_LOGD(TAG, "WiFi is not connected");
          wifi_status_data[1] = 1;
          wifi_status_data[3] = 0;
        }
        haier_protocol::HaierMessage wifi_status_request((uint8_t)hon_protocol::FrameType::REPORT_NETWORK_STATUS, wifi_status_data, sizeof(wifi_status_data));
        this->send_message(wifi_status_request);
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
      if (this->can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_request_timestamp_).count() > DEFAULT_MESSAGES_INTERVAL_MS))
      {
        static const haier_protocol::HaierMessage alarm_status_request((uint8_t)hon_protocol::FrameType::GET_ALARM_STATUS);
        this->send_message(alarm_status_request);
        this->set_phase(ProtocolPhases::WAITING_ALARM_STATUS_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_CONTROL:
      if (this->control_called_)
      {
        this->control_request_timestamp_ = now;
        this->control_called_ = false;
      }
      if (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->control_request_timestamp_).count() > CONTROL_TIMEOUT_MS)
      {
        ESP_LOGW(TAG, "Sending control packet timeout!");
        this->force_send_control_ = false;
        if (this->hvac_settings_.valid)
          this->hvac_settings_.reset();
        this->forced_request_status_ = true;
        this->forced_publish_ = true;
        this->set_phase(ProtocolPhases::IDLE);
      }
      else if (this->can_send_message() && (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_request_timestamp_).count() > CONTROL_MESSAGES_INTERVAL_MS)) // Usiing CONTROL_MESSAGES_INTERVAL_MS to speeduprequests
      {
        haier_protocol::HaierMessage control_message = get_control_message();
        this->send_message(control_message);
        ESP_LOGI(TAG, "Control packet sent");
        this->set_phase(ProtocolPhases::WAITING_CONTROL_ANSWER);       
      }
      break;
    case ProtocolPhases::WAITING_ANSWER_INIT_1:
    case ProtocolPhases::WAITING_ANSWER_INIT_2:
    case ProtocolPhases::WAITING_FIRST_STATUS_ANSWER:
    case ProtocolPhases::WAITING_ALARM_STATUS_ANSWER:
    case ProtocolPhases::WAITING_STATUS_ANSWER:
    case ProtocolPhases::WAITING_CONTROL_ANSWER:
      break;
    case ProtocolPhases::IDLE:
      {
        if (this->forced_request_status_ || (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_status_request_).count() > STATUS_REQUEST_INTERVAL_MS))
        {
          this->set_phase(ProtocolPhases::SENDING_STATUS_REQUEST);
          this->forced_request_status_ = false;
        }
#ifdef HAIER_REPORT_WIFI_SIGNAL
        else if (std::chrono::duration_cast<std::chrono::milliseconds>(now - this->last_signal_request_).count() > SIGNAL_LEVEL_UPDATE_INTERVAL_MS)
          this->set_phase(ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST);
#endif
      }
      break;
    default:
      // Shouldn't get here
#if (HAIER_LOG_LEVEL > 4)
      ESP_LOGE(TAG, "Wrong protocol handler state: %s (%d), resetting communication", phase_to_string_(this->protocol_phase_), (int)this->protocol_phase_);
#else
      ESP_LOGE(TAG, "Wrong protocol handler state: %d, resetting communication", (int)this->protocol_phase_);
#endif
      this->set_phase(ProtocolPhases::SENDING_INIT_1);
      break;
  }
  this->haier_protocol_.loop();
}

ClimateTraits HaierClimate::traits()
{
  return traits_;
}

void HaierClimate::control(const ClimateCall &call)
{
  ESP_LOGD("Control", "Control call");
  if (this->protocol_phase_ < ProtocolPhases::IDLE)
  {
    ESP_LOGW(TAG, "Can't send control packet, first poll answer not received");
    return; //cancel the control, we cant do it without a poll answer.
  }
  if (this->hvac_settings_.valid)
  {
    ESP_LOGW(TAG, "Overriding old valid settings before they were applied!");
  }
  {
    if (call.get_mode().has_value())
      this->hvac_settings_.mode = call.get_mode();
    if (call.get_fan_mode().has_value())
      this->hvac_settings_.fan_mode = call.get_fan_mode();
    if (call.get_swing_mode().has_value())
      this->hvac_settings_.swing_mode =  call.get_swing_mode();
    if (call.get_target_temperature().has_value())
      this->hvac_settings_.target_temperature = call.get_target_temperature();
    if (call.get_preset().has_value())
      this->hvac_settings_.preset = call.get_preset();
    this->hvac_settings_.valid = true;
  }
  this->control_called_ = true;
}
const haier_protocol::HaierMessage HaierClimate::get_control_message()
{
  uint8_t controlOutBuffer[sizeof(hon_protocol::HaierPacketControl)];
  memcpy(controlOutBuffer, this->last_status_message_, sizeof(hon_protocol::HaierPacketControl));
  hon_protocol::HaierPacketControl* outData = (hon_protocol::HaierPacketControl*)controlOutBuffer;
  bool hasHvacSettings = false;
  if (this->hvac_settings_.valid)
  {
    hasHvacSettings = true;
    HvacSettings climateControl;
    climateControl = this->hvac_settings_;
    if (climateControl.mode.has_value())
    {
      switch (climateControl.mode.value())
      {
      case CLIMATE_MODE_OFF:
        outData->ac_power = 0;
        break;

      case CLIMATE_MODE_AUTO:
        outData->ac_power = 1;
        outData->ac_mode = (uint8_t)hon_protocol::ConditioningMode::AUTO;
        outData->fan_mode = this->other_modes_fan_speed_;
        break;

      case CLIMATE_MODE_HEAT:
        outData->ac_power = 1;
        outData->ac_mode = (uint8_t)hon_protocol::ConditioningMode::HEAT;
        outData->fan_mode = this->other_modes_fan_speed_;
        break;

      case CLIMATE_MODE_DRY:
        outData->ac_power = 1;
        outData->ac_mode = (uint8_t)hon_protocol::ConditioningMode::DRY;
        outData->fan_mode = this->other_modes_fan_speed_;
        break;

      case CLIMATE_MODE_FAN_ONLY:
        outData->ac_power = 1;
        outData->ac_mode = (uint8_t)hon_protocol::ConditioningMode::FAN;
        outData->fan_mode = this->fan_mode_speed_;    // Auto doesn't work in fan only mode
        break;

      case CLIMATE_MODE_COOL:
        outData->ac_power = 1;
        outData->ac_mode = (uint8_t)hon_protocol::ConditioningMode::COOL;
        outData->fan_mode = this->other_modes_fan_speed_;
        break;
      default:
        ESP_LOGE("Control", "Unsupported climate mode");
        break;
      }
    }
    //Set fan speed, if we are in fan mode, reject auto in fan mode
    if (climateControl.fan_mode.has_value())
    {
      switch (climateControl.fan_mode.value())
      {
      case CLIMATE_FAN_LOW:
        outData->fan_mode = (uint8_t)hon_protocol::FanMode::FAN_LOW;
        break;
      case CLIMATE_FAN_MEDIUM:
        outData->fan_mode = (uint8_t)hon_protocol::FanMode::FAN_MID;
        break;
      case CLIMATE_FAN_HIGH:
        outData->fan_mode = (uint8_t)hon_protocol::FanMode::FAN_HIGH;
        break;
      case CLIMATE_FAN_AUTO:
        if (mode != CLIMATE_MODE_FAN_ONLY) //if we are not in fan only mode
          outData->fan_mode = (uint8_t)hon_protocol::FanMode::FAN_AUTO;
        break;
      default:
        ESP_LOGE("Control", "Unsupported fan mode");
        break;
      }
    }
    //Set swing mode
    if (climateControl.swing_mode.has_value())
    {
      switch (climateControl.swing_mode.value())
      {
      case CLIMATE_SWING_OFF:
        outData->horizontal_swing_mode = (uint8_t)get_horizontal_swing_mode(this->horizontal_direction_);
        outData->vertical_swing_mode = (uint8_t)get_vertical_swing_mode(this->vertical_direction_);
        break;
      case CLIMATE_SWING_VERTICAL:
        outData->horizontal_swing_mode = (uint8_t)get_horizontal_swing_mode(this->horizontal_direction_);
        outData->vertical_swing_mode = (uint8_t)hon_protocol::VerticalSwingMode::AUTO;
        break;
      case CLIMATE_SWING_HORIZONTAL:
        outData->horizontal_swing_mode = (uint8_t)hon_protocol::HorizontalSwingMode::AUTO;
        outData->vertical_swing_mode = (uint8_t)get_vertical_swing_mode(this->vertical_direction_);
        break;
      case CLIMATE_SWING_BOTH:
        outData->horizontal_swing_mode = (uint8_t)hon_protocol::HorizontalSwingMode::AUTO;
        outData->vertical_swing_mode = (uint8_t)hon_protocol::VerticalSwingMode::AUTO;
        break;
      }
    }
    if (climateControl.target_temperature.has_value())
      outData->set_point = climateControl.target_temperature.value() - 16; //set the temperature at our offset, subtract 16.
    if (climateControl.preset.has_value())
    {
      switch (climateControl.preset.value())
      {
      case CLIMATE_PRESET_NONE:
        outData->quiet_mode = 0;
        outData->fast_mode = 0;
        outData->sleep_mode = 0;
        break;
      case CLIMATE_PRESET_ECO:
        outData->quiet_mode = 1;
        outData->fast_mode = 0;
        outData->sleep_mode = 0;
        break;
      case CLIMATE_PRESET_BOOST:
        outData->quiet_mode = 0;
        outData->fast_mode = 1;
        outData->sleep_mode = 0;
        break;
      case CLIMATE_PRESET_AWAY:
        outData->quiet_mode = 0;
        outData->fast_mode = 0;
        outData->sleep_mode = 0;
        break;
      case CLIMATE_PRESET_SLEEP:
        outData->quiet_mode = 0;
        outData->fast_mode = 0;
        outData->sleep_mode = 1;
        break;
      default:
        ESP_LOGE("Control", "Unsupported preset");
        break;
      }
    }
  }
  else
  {
    if (outData->vertical_swing_mode != (uint8_t)hon_protocol::VerticalSwingMode::AUTO)
      outData->vertical_swing_mode = (uint8_t)get_vertical_swing_mode(this->vertical_direction_);
    if (outData->horizontal_swing_mode != (uint8_t)hon_protocol::HorizontalSwingMode::AUTO)
      outData->horizontal_swing_mode = (uint8_t)get_horizontal_swing_mode(this->horizontal_direction_);
  }
  outData->beeper_status = ((!this->beeper_status_) || (!hasHvacSettings)) ? 1 : 0;
  controlOutBuffer[4] = 0;   // This byte should be cleared before setting values
  outData->display_status = this->display_status_ ? 1 : 0;
  const haier_protocol::HaierMessage control_message((uint8_t)hon_protocol::FrameType::CONTROL, (uint16_t)hon_protocol::SubcomandsControl::SET_GROUP_PARAMETERS, controlOutBuffer, sizeof(hon_protocol::HaierPacketControl));
  return control_message;
}

haier_protocol::HandlerError HaierClimate::process_status_message(const uint8_t* packetBuffer, uint8_t size)
{
  if (size < sizeof(hon_protocol::HaierStatus))
    return haier_protocol::HandlerError::WRONG_MESSAGE_STRUCTURE;
  hon_protocol::HaierStatus packet;
  if (size < sizeof(hon_protocol::HaierStatus))
    size = sizeof(hon_protocol::HaierStatus);
  memcpy(&packet, packetBuffer, size);
  if (packet.sensors.error_status != 0)
  {
    ESP_LOGW(TAG, "HVAC error, code=0x%02X message: %s",
      packet.sensors.error_status,
      (packet.sensors.error_status < sizeof(hon_protocol::ErrorMessages)) ? hon_protocol::ErrorMessages[packet.sensors.error_status].c_str() : "Unknown error");
  }
  if ((this->outdoor_sensor_ != nullptr) && (got_valid_outdoor_temp_ || (packet.sensors.outdoor_temperature > 0)))
  {
    got_valid_outdoor_temp_ = true; 
    float otemp = (float)(packet.sensors.outdoor_temperature + PROTOCOL_OUTDOOR_TEMPERATURE_OFFSET);
    if ((!this->outdoor_sensor_->has_state()) || (this->outdoor_sensor_->get_raw_state() != otemp))
      this->outdoor_sensor_->publish_state(otemp);
  }
  bool shouldPublish = false;    
  {
    // Extra modes/presets
    optional<ClimatePreset> oldPreset = this->preset;
    if (packet.control.quiet_mode != 0)
    {
      this->preset = CLIMATE_PRESET_ECO;
    }
    else if (packet.control.fast_mode != 0)
    {
      this->preset = CLIMATE_PRESET_BOOST;
    }
    else if (packet.control.sleep_mode != 0)
    {
      this->preset = CLIMATE_PRESET_SLEEP;
    }
    else
    {
      this->preset = CLIMATE_PRESET_NONE;
    }
    shouldPublish = shouldPublish || (!oldPreset.has_value()) || (oldPreset.value() != this->preset.value());
  }
  {
    // Target temperature
    float oldTargetTemperature = this->target_temperature;
    this->target_temperature = packet.control.set_point + 16.0f;
    shouldPublish = shouldPublish || (oldTargetTemperature != this->target_temperature);
  }
  {
    // Current temperature
    float oldCurrentTemperature = this->current_temperature;
    this->current_temperature = packet.sensors.room_temperature / 2.0f;
    shouldPublish = shouldPublish || (oldCurrentTemperature != this->current_temperature);
  }
  {
    // Fan mode
    optional<ClimateFanMode> oldFanMode = this->fan_mode;
    //remember the fan speed we last had for climate vs fan
    if (packet.control.ac_mode == (uint8_t)hon_protocol::ConditioningMode::FAN)
      this->fan_mode_speed_ = packet.control.fan_mode;
    else
      this->other_modes_fan_speed_ = packet.control.fan_mode;
    switch (packet.control.fan_mode)
    {
    case (uint8_t)hon_protocol::FanMode::FAN_AUTO:
      this->fan_mode = CLIMATE_FAN_AUTO;
      break;
    case (uint8_t)hon_protocol::FanMode::FAN_MID:
      this->fan_mode = CLIMATE_FAN_MEDIUM;
      break;
    case (uint8_t)hon_protocol::FanMode::FAN_LOW:
      this->fan_mode = CLIMATE_FAN_LOW;
      break;
    case (uint8_t)hon_protocol::FanMode::FAN_HIGH:
      this->fan_mode = CLIMATE_FAN_HIGH;
      break;
    }
    shouldPublish = shouldPublish || (!oldFanMode.has_value()) || (oldFanMode.value() != fan_mode.value());
  }
  {
    // Climate mode
    ClimateMode oldMode = this->mode;
    if (packet.control.ac_power == 0)
      this->mode = CLIMATE_MODE_OFF;
    else
    {
      // Check current hvac mode
      switch (packet.control.ac_mode)
      {
      case (uint8_t)hon_protocol::ConditioningMode::COOL:
        this->mode = CLIMATE_MODE_COOL;
        break;
      case (uint8_t)hon_protocol::ConditioningMode::HEAT:
        this->mode = CLIMATE_MODE_HEAT;
        break;
      case (uint8_t)hon_protocol::ConditioningMode::DRY:
        this->mode = CLIMATE_MODE_DRY;
        break;
      case (uint8_t)hon_protocol::ConditioningMode::FAN:
        this->mode = CLIMATE_MODE_FAN_ONLY;
        break;
      case (uint8_t)hon_protocol::ConditioningMode::AUTO:
        this->mode = CLIMATE_MODE_AUTO;
        break;
      }
    }
    shouldPublish = shouldPublish || (oldMode != this->mode);
  }
  {
    // Swing mode
    ClimateSwingMode oldSwingMode = this->swing_mode;
    if (packet.control.horizontal_swing_mode == (uint8_t)hon_protocol::HorizontalSwingMode::AUTO)
    {
      if (packet.control.vertical_swing_mode == (uint8_t)hon_protocol::VerticalSwingMode::AUTO)
        this->swing_mode = CLIMATE_SWING_BOTH;
      else
        this->swing_mode = CLIMATE_SWING_HORIZONTAL;
    }
    else
    {
      if (packet.control.vertical_swing_mode == (uint8_t)hon_protocol::VerticalSwingMode::AUTO)
        this->swing_mode = CLIMATE_SWING_VERTICAL;
      else
        this->swing_mode = CLIMATE_SWING_OFF;
    }
    shouldPublish = shouldPublish || (oldSwingMode != this->swing_mode);
  }
  this->last_valid_status_timestamp_ = std::chrono::steady_clock::now();
  if (this->forced_publish_ || shouldPublish)
  {
#if (HAIER_LOG_LEVEL > 4)
    std::chrono::high_resolution_clock::time_point _publish_start = std::chrono::high_resolution_clock::now();
#endif
    this->publish_state();
#if (HAIER_LOG_LEVEL > 4)
    ESP_LOGV(TAG, "Publish delay: %lld ms", std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - _publish_start).count());
#endif
    this->forced_publish_ = false;
  }
  if (shouldPublish)
  {
    ESP_LOGI(TAG, "HVAC values changed");
  }
  esp_log_printf_((shouldPublish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__, "HVAC Mode = 0x%X", packet.control.ac_mode);
  esp_log_printf_((shouldPublish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__, "Fan speed Status = 0x%X", packet.control.fan_mode);
  esp_log_printf_((shouldPublish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__, "Horizontal Swing Status = 0x%X", packet.control.horizontal_swing_mode);
  esp_log_printf_((shouldPublish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__, "Vertical Swing Status = 0x%X", packet.control.vertical_swing_mode);
  esp_log_printf_((shouldPublish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__, "Set Point Status = 0x%X", packet.control.set_point);
  return haier_protocol::HandlerError::HANDLER_OK;
}

void HaierClimate::send_message(const haier_protocol::HaierMessage& command)
{
  this->haier_protocol_.send_message(command, this->use_crc_);
  this->last_request_timestamp_ = std::chrono::steady_clock::now();;
}

void HaierClimate::HvacSettings::reset()
{
  this->valid = false;
  this->mode.reset();
  this->fan_mode.reset();
  this->swing_mode.reset();
  this->target_temperature.reset();
  this->preset.reset();
}

} // namespace haier
} // namespace esphome
