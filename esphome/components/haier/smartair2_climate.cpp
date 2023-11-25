#include <chrono>
#include "esphome/components/climate/climate.h"
#include "esphome/components/uart/uart.h"
#include "smartair2_climate.h"
#include "smartair2_packet.h"

using namespace esphome::climate;
using namespace esphome::uart;

namespace esphome {
namespace haier {

static const char *const TAG = "haier.climate";
constexpr size_t SIGNAL_LEVEL_UPDATE_INTERVAL_MS = 10000;
constexpr uint8_t CONTROL_MESSAGE_RETRIES = 5;
constexpr std::chrono::milliseconds CONTROL_MESSAGE_RETRIES_INTERVAL = std::chrono::milliseconds(500);
constexpr uint8_t INIT_REQUESTS_RETRY = 2;
constexpr std::chrono::milliseconds INIT_REQUESTS_RETRY_INTERVAL = std::chrono::milliseconds(2000);

Smartair2Climate::Smartair2Climate() {
  last_status_message_ = std::unique_ptr<uint8_t[]>(new uint8_t[sizeof(smartair2_protocol::HaierPacketControl)]);
}

haier_protocol::HandlerError Smartair2Climate::status_handler_(haier_protocol::FrameType request_type,
                                                               haier_protocol::FrameType message_type,
                                                               const uint8_t *data, size_t data_size) {
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
      if (data_size >= sizeof(smartair2_protocol::HaierPacketControl) + 2) {
        memcpy(this->last_status_message_.get(), data + 2, sizeof(smartair2_protocol::HaierPacketControl));
      } else {
        ESP_LOGW(TAG, "Status packet too small: %d (should be >= %d)", data_size,
                 sizeof(smartair2_protocol::HaierPacketControl));
      }
      switch (this->protocol_phase_) {
        case ProtocolPhases::SENDING_FIRST_STATUS_REQUEST:
          ESP_LOGI(TAG, "First HVAC status received");
          this->set_phase(ProtocolPhases::IDLE);
          break;
        case ProtocolPhases::SENDING_ACTION_COMMAND:
          // Do nothing, phase will be changed in process_phase
          break;
        case ProtocolPhases::SENDING_STATUS_REQUEST:
          this->set_phase(ProtocolPhases::IDLE);
          break;
        case ProtocolPhases::SENDING_CONTROL:
          this->set_phase(ProtocolPhases::IDLE);
          this->force_send_control_ = false;
          if (this->current_hvac_settings_.valid)
            this->current_hvac_settings_.reset();
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

haier_protocol::HandlerError Smartair2Climate::get_device_version_answer_handler_(
    haier_protocol::FrameType request_type, haier_protocol::FrameType message_type, const uint8_t *data,
    size_t data_size) {
  if (request_type != haier_protocol::FrameType::GET_DEVICE_VERSION)
    return haier_protocol::HandlerError::UNSUPPORTED_MESSAGE;
  if (ProtocolPhases::SENDING_INIT_1 != this->protocol_phase_)
    return haier_protocol::HandlerError::UNEXPECTED_MESSAGE;
  // Invalid packet is expected answer
  if ((message_type == haier_protocol::FrameType::GET_DEVICE_VERSION_RESPONSE) && (data_size >= 39) &&
      ((data[37] & 0x04) != 0)) {
    ESP_LOGW(TAG, "It looks like your ESPHome Haier climate configuration is wrong. You should use the hOn protocol "
                  "instead of smartAir2");
  }
  this->set_phase(ProtocolPhases::SENDING_INIT_2);
  return haier_protocol::HandlerError::HANDLER_OK;
}

haier_protocol::HandlerError Smartair2Climate::messages_timeout_handler_with_cycle_for_init_(
    haier_protocol::FrameType message_type) {
  if (this->protocol_phase_ >= ProtocolPhases::IDLE)
    return HaierClimateBase::timeout_default_handler_(message_type);
  ESP_LOGI(TAG, "Answer timeout for command %02X, phase %s", (uint8_t) message_type,
           phase_to_string_(this->protocol_phase_));
  ProtocolPhases new_phase = (ProtocolPhases) ((int) this->protocol_phase_ + 1);
  if (new_phase >= ProtocolPhases::SENDING_ALARM_STATUS_REQUEST)
    new_phase = ProtocolPhases::SENDING_INIT_1;
  this->set_phase(new_phase);
  return haier_protocol::HandlerError::HANDLER_OK;
}

void Smartair2Climate::set_handlers() {
  // Set handlers
  this->haier_protocol_.set_answer_handler(
      haier_protocol::FrameType::GET_DEVICE_VERSION,
      std::bind(&Smartair2Climate::get_device_version_answer_handler_, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      haier_protocol::FrameType::CONTROL,
      std::bind(&Smartair2Climate::status_handler_, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_answer_handler(
      haier_protocol::FrameType::REPORT_NETWORK_STATUS,
      std::bind(&Smartair2Climate::report_network_status_answer_handler_, this, std::placeholders::_1,
                std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));
  this->haier_protocol_.set_default_timeout_handler(
      std::bind(&Smartair2Climate::messages_timeout_handler_with_cycle_for_init_, this, std::placeholders::_1));
}

void Smartair2Climate::dump_config() {
  HaierClimateBase::dump_config();
  ESP_LOGCONFIG(TAG, "  Protocol version: smartAir2");
}

void Smartair2Climate::process_phase(std::chrono::steady_clock::time_point now) {
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
        this->send_message_(DEVICE_VERSION_REQUEST, this->use_crc_, INIT_REQUESTS_RETRY, INIT_REQUESTS_RETRY_INTERVAL);
      }
      break;
    case ProtocolPhases::SENDING_INIT_2:
      this->set_phase(ProtocolPhases::SENDING_FIRST_STATUS_REQUEST);
      break;
    case ProtocolPhases::SENDING_FIRST_STATUS_REQUEST:
    case ProtocolPhases::SENDING_STATUS_REQUEST:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        static const haier_protocol::HaierMessage STATUS_REQUEST(haier_protocol::FrameType::CONTROL, 0x4D01);
        if (this->protocol_phase_ == ProtocolPhases::SENDING_FIRST_STATUS_REQUEST) {
          this->send_message_(STATUS_REQUEST, this->use_crc_, INIT_REQUESTS_RETRY, INIT_REQUESTS_RETRY_INTERVAL);
        } else {
          this->send_message_(STATUS_REQUEST, this->use_crc_);
        }
        this->last_status_request_ = now;
      }
      break;
#ifdef USE_WIFI
    case ProtocolPhases::SENDING_SIGNAL_LEVEL:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        this->send_message_(this->get_wifi_signal_message_(), this->use_crc_);
        this->last_signal_request_ = now;
      }
      break;
#else
    case ProtocolPhases::SENDING_SIGNAL_LEVEL:
      this->set_phase(ProtocolPhases::IDLE);
      break;
#endif
    case ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST:
      this->set_phase(ProtocolPhases::SENDING_SIGNAL_LEVEL);
      break;
    case ProtocolPhases::SENDING_ALARM_STATUS_REQUEST:
      this->set_phase(ProtocolPhases::SENDING_INIT_1);
      break;
    case ProtocolPhases::SENDING_CONTROL:
      if (this->can_send_message() && this->is_control_message_interval_exceeded_(now)) {
        ESP_LOGI(TAG, "Sending control packet");
        this->send_message_(get_control_message(), this->use_crc_, CONTROL_MESSAGE_RETRIES,
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
      ESP_LOGE(TAG, "Wrong protocol handler state: %s (%d), resetting communication",
               phase_to_string_(this->protocol_phase_), (int) this->protocol_phase_);
      this->set_phase(ProtocolPhases::SENDING_INIT_1);
      break;
  }
}

haier_protocol::HaierMessage Smartair2Climate::get_power_message(bool state) {
  if (state) {
    static haier_protocol::HaierMessage power_on_message(haier_protocol::FrameType::CONTROL, 0x4D02);
    return power_on_message;
  } else {
    static haier_protocol::HaierMessage power_off_message(haier_protocol::FrameType::CONTROL, 0x4D03);
    return power_off_message;
  }
}

haier_protocol::HaierMessage Smartair2Climate::get_control_message() {
  uint8_t control_out_buffer[sizeof(smartair2_protocol::HaierPacketControl)];
  memcpy(control_out_buffer, this->last_status_message_.get(), sizeof(smartair2_protocol::HaierPacketControl));
  smartair2_protocol::HaierPacketControl *out_data = (smartair2_protocol::HaierPacketControl *) control_out_buffer;
  out_data->cntrl = 0;
  if (this->current_hvac_settings_.valid) {
    HvacSettings &climate_control = this->current_hvac_settings_;
    if (climate_control.mode.has_value()) {
      switch (climate_control.mode.value()) {
        case CLIMATE_MODE_OFF:
          out_data->ac_power = 0;
          break;
        case CLIMATE_MODE_HEAT_COOL:
          out_data->ac_power = 1;
          out_data->ac_mode = (uint8_t) smartair2_protocol::ConditioningMode::AUTO;
          out_data->fan_mode = this->other_modes_fan_speed_;
          break;
        case CLIMATE_MODE_HEAT:
          out_data->ac_power = 1;
          out_data->ac_mode = (uint8_t) smartair2_protocol::ConditioningMode::HEAT;
          out_data->fan_mode = this->other_modes_fan_speed_;
          break;
        case CLIMATE_MODE_DRY:
          out_data->ac_power = 1;
          out_data->ac_mode = (uint8_t) smartair2_protocol::ConditioningMode::DRY;
          out_data->fan_mode = this->other_modes_fan_speed_;
          break;
        case CLIMATE_MODE_FAN_ONLY:
          out_data->ac_power = 1;
          out_data->ac_mode = (uint8_t) smartair2_protocol::ConditioningMode::FAN;
          out_data->fan_mode = this->fan_mode_speed_;  // Auto doesn't work in fan only mode
          break;
        case CLIMATE_MODE_COOL:
          out_data->ac_power = 1;
          out_data->ac_mode = (uint8_t) smartair2_protocol::ConditioningMode::COOL;
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
          out_data->fan_mode = (uint8_t) smartair2_protocol::FanMode::FAN_LOW;
          break;
        case CLIMATE_FAN_MEDIUM:
          out_data->fan_mode = (uint8_t) smartair2_protocol::FanMode::FAN_MID;
          break;
        case CLIMATE_FAN_HIGH:
          out_data->fan_mode = (uint8_t) smartair2_protocol::FanMode::FAN_HIGH;
          break;
        case CLIMATE_FAN_AUTO:
          if (this->mode != CLIMATE_MODE_FAN_ONLY)  // if we are not in fan only mode
            out_data->fan_mode = (uint8_t) smartair2_protocol::FanMode::FAN_AUTO;
          break;
        default:
          ESP_LOGE("Control", "Unsupported fan mode");
          break;
      }
    }
    // Set swing mode
    if (climate_control.swing_mode.has_value()) {
      if (this->use_alternative_swing_control_) {
        switch (climate_control.swing_mode.value()) {
          case CLIMATE_SWING_OFF:
            out_data->swing_mode = 0;
            break;
          case CLIMATE_SWING_VERTICAL:
            out_data->swing_mode = 1;
            break;
          case CLIMATE_SWING_HORIZONTAL:
            out_data->swing_mode = 2;
            break;
          case CLIMATE_SWING_BOTH:
            out_data->swing_mode = 3;
            break;
        }
      } else {
        switch (climate_control.swing_mode.value()) {
          case CLIMATE_SWING_OFF:
            out_data->use_swing_bits = 0;
            out_data->swing_mode = 0;
            break;
          case CLIMATE_SWING_VERTICAL:
            out_data->swing_mode = 0;
            out_data->vertical_swing = 1;
            out_data->horizontal_swing = 0;
            break;
          case CLIMATE_SWING_HORIZONTAL:
            out_data->swing_mode = 0;
            out_data->vertical_swing = 0;
            out_data->horizontal_swing = 1;
            break;
          case CLIMATE_SWING_BOTH:
            out_data->swing_mode = 1;
            out_data->use_swing_bits = 0;
            out_data->vertical_swing = 0;
            out_data->horizontal_swing = 0;
            break;
        }
      }
    }
    if (climate_control.target_temperature.has_value()) {
      float target_temp = climate_control.target_temperature.value();
      out_data->set_point = ((int) target_temp) - 16;  // set the temperature with offset 16
      out_data->half_degree = (target_temp - ((int) target_temp) >= 0.49) ? 1 : 0;
    }
    if (out_data->ac_power == 0) {
      // If AC is off - no presets allowed
      out_data->turbo_mode = 0;
      out_data->quiet_mode = 0;
    } else if (climate_control.preset.has_value()) {
      switch (climate_control.preset.value()) {
        case CLIMATE_PRESET_NONE:
          out_data->turbo_mode = 0;
          out_data->quiet_mode = 0;
          break;
        case CLIMATE_PRESET_BOOST:
          out_data->turbo_mode = 1;
          out_data->quiet_mode = 0;
          break;
        case CLIMATE_PRESET_COMFORT:
          out_data->turbo_mode = 0;
          out_data->quiet_mode = 1;
          break;
        default:
          ESP_LOGE("Control", "Unsupported preset");
          out_data->turbo_mode = 0;
          out_data->quiet_mode = 0;
          break;
      }
    }
  }
  out_data->display_status = this->display_status_ ? 0 : 1;
  out_data->health_mode = this->health_mode_ ? 1 : 0;
  return haier_protocol::HaierMessage(haier_protocol::FrameType::CONTROL, 0x4D5F, control_out_buffer,
                                      sizeof(smartair2_protocol::HaierPacketControl));
}

haier_protocol::HandlerError Smartair2Climate::process_status_message_(const uint8_t *packet_buffer, uint8_t size) {
  if (size < sizeof(smartair2_protocol::HaierStatus))
    return haier_protocol::HandlerError::WRONG_MESSAGE_STRUCTURE;
  smartair2_protocol::HaierStatus packet;
  memcpy(&packet, packet_buffer, size);
  bool should_publish = false;
  {
    // Extra modes/presets
    optional<ClimatePreset> old_preset = this->preset;
    if (packet.control.turbo_mode != 0) {
      this->preset = CLIMATE_PRESET_BOOST;
    } else if (packet.control.quiet_mode != 0) {
      this->preset = CLIMATE_PRESET_COMFORT;
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
    this->current_temperature = packet.control.room_temperature;
    should_publish = should_publish || (old_current_temperature != this->current_temperature);
  }
  {
    // Fan mode
    optional<ClimateFanMode> old_fan_mode = this->fan_mode;
    // remember the fan speed we last had for climate vs fan
    if (packet.control.ac_mode == (uint8_t) smartair2_protocol::ConditioningMode::FAN) {
      if (packet.control.fan_mode != (uint8_t) smartair2_protocol::FanMode::FAN_AUTO)
        this->fan_mode_speed_ = packet.control.fan_mode;
    } else {
      this->other_modes_fan_speed_ = packet.control.fan_mode;
    }
    switch (packet.control.fan_mode) {
      case (uint8_t) smartair2_protocol::FanMode::FAN_AUTO:
        // Sometimes AC reports in fan only mode that fan speed is auto
        // but never accept this value back
        if (packet.control.ac_mode != (uint8_t) smartair2_protocol::ConditioningMode::FAN) {
          this->fan_mode = CLIMATE_FAN_AUTO;
        } else {
          should_publish = true;
        }
        break;
      case (uint8_t) smartair2_protocol::FanMode::FAN_MID:
        this->fan_mode = CLIMATE_FAN_MEDIUM;
        break;
      case (uint8_t) smartair2_protocol::FanMode::FAN_LOW:
        this->fan_mode = CLIMATE_FAN_LOW;
        break;
      case (uint8_t) smartair2_protocol::FanMode::FAN_HIGH:
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
      bool disp_status = packet.control.display_status == 0;
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
    // Climate mode
    ClimateMode old_mode = this->mode;
    if (packet.control.ac_power == 0) {
      this->mode = CLIMATE_MODE_OFF;
    } else {
      // Check current hvac mode
      switch (packet.control.ac_mode) {
        case (uint8_t) smartair2_protocol::ConditioningMode::COOL:
          this->mode = CLIMATE_MODE_COOL;
          break;
        case (uint8_t) smartair2_protocol::ConditioningMode::HEAT:
          this->mode = CLIMATE_MODE_HEAT;
          break;
        case (uint8_t) smartair2_protocol::ConditioningMode::DRY:
          this->mode = CLIMATE_MODE_DRY;
          break;
        case (uint8_t) smartair2_protocol::ConditioningMode::FAN:
          this->mode = CLIMATE_MODE_FAN_ONLY;
          break;
        case (uint8_t) smartair2_protocol::ConditioningMode::AUTO:
          this->mode = CLIMATE_MODE_HEAT_COOL;
          break;
      }
    }
    should_publish = should_publish || (old_mode != this->mode);
  }
  {
    // Swing mode
    ClimateSwingMode old_swing_mode = this->swing_mode;
    if (this->use_alternative_swing_control_) {
      switch (packet.control.swing_mode) {
        case 1:
          this->swing_mode = CLIMATE_SWING_VERTICAL;
          break;
        case 2:
          this->swing_mode = CLIMATE_SWING_HORIZONTAL;
          break;
        case 3:
          this->swing_mode = CLIMATE_SWING_BOTH;
          break;
        default:
          this->swing_mode = CLIMATE_SWING_OFF;
          break;
      }
    } else {
      if (packet.control.swing_mode == 0) {
        if (packet.control.vertical_swing != 0) {
          this->swing_mode = CLIMATE_SWING_VERTICAL;
        } else if (packet.control.horizontal_swing != 0) {
          this->swing_mode = CLIMATE_SWING_HORIZONTAL;
        } else {
          this->swing_mode = CLIMATE_SWING_OFF;
        }
      } else {
        swing_mode = CLIMATE_SWING_BOTH;
      }
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
  esp_log_printf_(log_level, TAG, __LINE__, "Horizontal Swing Status = 0x%X", packet.control.horizontal_swing);
  esp_log_printf_(log_level, TAG, __LINE__, "Vertical Swing Status = 0x%X", packet.control.vertical_swing);
  esp_log_printf_(log_level, TAG, __LINE__, "Set Point Status = 0x%X", packet.control.set_point);
  return haier_protocol::HandlerError::HANDLER_OK;
}

void Smartair2Climate::set_alternative_swing_control(bool swing_control) {
  this->use_alternative_swing_control_ = swing_control;
}

}  // namespace haier
}  // namespace esphome
