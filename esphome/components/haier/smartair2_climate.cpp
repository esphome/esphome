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

Smartair2Climate::Smartair2Climate()
    : last_status_message_(new uint8_t[sizeof(smartair2_protocol::HaierPacketControl)]) {
  this->traits_.set_supported_presets({
      climate::CLIMATE_PRESET_NONE,
      climate::CLIMATE_PRESET_BOOST,
      climate::CLIMATE_PRESET_COMFORT,
  });
}

haier_protocol::HandlerError Smartair2Climate::status_handler_(uint8_t request_type, uint8_t message_type,
                                                               const uint8_t *data, size_t data_size) {
  haier_protocol::HandlerError result =
      this->answer_preprocess_(request_type, (uint8_t) smartair2_protocol::FrameType::CONTROL, message_type,
                               (uint8_t) smartair2_protocol::FrameType::STATUS, ProtocolPhases::UNKNOWN);
  if (result == haier_protocol::HandlerError::HANDLER_OK) {
    result = this->process_status_message_(data, data_size);
    if (result != haier_protocol::HandlerError::HANDLER_OK) {
      ESP_LOGW(TAG, "Error %d while parsing Status packet", (int) result);
      this->set_phase_((this->protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE
                                                                       : ProtocolPhases::SENDING_FIRST_STATUS_REQUEST);
    } else {
      if (data_size >= sizeof(smartair2_protocol::HaierPacketControl) + 2) {
        memcpy(this->last_status_message_.get(), data + 2, sizeof(smartair2_protocol::HaierPacketControl));
      } else {
        ESP_LOGW(TAG, "Status packet too small: %d (should be >= %d)", data_size,
                 sizeof(smartair2_protocol::HaierPacketControl));
      }
      if (this->protocol_phase_ == ProtocolPhases::WAITING_FIRST_STATUS_ANSWER) {
        ESP_LOGI(TAG, "First HVAC status received");
        this->set_phase_(ProtocolPhases::IDLE);
      } else if (this->protocol_phase_ == ProtocolPhases::WAITING_STATUS_ANSWER) {
        this->set_phase_(ProtocolPhases::IDLE);
      } else if (this->protocol_phase_ == ProtocolPhases::WAITING_CONTROL_ANSWER) {
        this->set_phase_(ProtocolPhases::IDLE);
        this->set_force_send_control_(false);
        if (this->hvac_settings_.valid)
          this->hvac_settings_.reset();
      }
    }
    return result;
  } else {
    this->set_phase_((this->protocol_phase_ >= ProtocolPhases::IDLE) ? ProtocolPhases::IDLE
                                                                     : ProtocolPhases::SENDING_FIRST_STATUS_REQUEST);
    return result;
  }
}

void Smartair2Climate::set_answers_handlers() {
  this->haier_protocol_.set_answer_handler(
      (uint8_t) (smartair2_protocol::FrameType::CONTROL),
      std::bind(&Smartair2Climate::status_handler_, this, std::placeholders::_1, std::placeholders::_2,
                std::placeholders::_3, std::placeholders::_4));
}

void Smartair2Climate::dump_config() {
  HaierClimateBase::dump_config();
  ESP_LOGCONFIG(TAG, "  Protocol version: smartAir2");
}

void Smartair2Climate::process_phase(std::chrono::steady_clock::time_point now) {
  switch (this->protocol_phase_) {
    case ProtocolPhases::SENDING_INIT_1:
      this->set_phase_(ProtocolPhases::SENDING_FIRST_STATUS_REQUEST);
      break;
    case ProtocolPhases::WAITING_ANSWER_INIT_1:
    case ProtocolPhases::SENDING_INIT_2:
    case ProtocolPhases::WAITING_ANSWER_INIT_2:
    case ProtocolPhases::SENDING_ALARM_STATUS_REQUEST:
    case ProtocolPhases::WAITING_ALARM_STATUS_ANSWER:
      this->set_phase_(ProtocolPhases::SENDING_INIT_1);
      break;
    case ProtocolPhases::SENDING_UPDATE_SIGNAL_REQUEST:
    case ProtocolPhases::WAITING_UPDATE_SIGNAL_ANSWER:
    case ProtocolPhases::SENDING_SIGNAL_LEVEL:
    case ProtocolPhases::WAITING_SIGNAL_LEVEL_ANSWER:
      this->set_phase_(ProtocolPhases::IDLE);
      break;
    case ProtocolPhases::SENDING_FIRST_STATUS_REQUEST:
      if (this->can_send_message() && this->is_protocol_initialisation_interval_exceded_(now)) {
        static const haier_protocol::HaierMessage STATUS_REQUEST((uint8_t) smartair2_protocol::FrameType::CONTROL,
                                                                 0x4D01);
        this->send_message_(STATUS_REQUEST, false);
        this->last_status_request_ = now;
        this->set_phase_(ProtocolPhases::WAITING_FIRST_STATUS_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_STATUS_REQUEST:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        static const haier_protocol::HaierMessage STATUS_REQUEST((uint8_t) smartair2_protocol::FrameType::CONTROL,
                                                                 0x4D01);
        this->send_message_(STATUS_REQUEST, false);
        this->last_status_request_ = now;
        this->set_phase_(ProtocolPhases::WAITING_STATUS_ANSWER);
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
        this->set_phase_(ProtocolPhases::IDLE);
      } else if (this->can_send_message() && this->is_control_message_interval_exceeded_(
                                                 now))  // Using CONTROL_MESSAGES_INTERVAL_MS to speedup requests
      {
        haier_protocol::HaierMessage control_message = get_control_message();
        this->send_message_(control_message, false);
        ESP_LOGI(TAG, "Control packet sent");
        this->set_phase_(ProtocolPhases::WAITING_CONTROL_ANSWER);
      }
      break;
    case ProtocolPhases::SENDING_POWER_ON_COMMAND:
    case ProtocolPhases::SENDING_POWER_OFF_COMMAND:
      if (this->can_send_message() && this->is_message_interval_exceeded_(now)) {
        haier_protocol::HaierMessage power_cmd(
            (uint8_t) smartair2_protocol::FrameType::CONTROL,
            this->protocol_phase_ == ProtocolPhases::SENDING_POWER_ON_COMMAND ? 0x4D02 : 0x4D03);
        this->send_message_(power_cmd, false);
        this->set_phase_(this->protocol_phase_ == ProtocolPhases::SENDING_POWER_ON_COMMAND
                             ? ProtocolPhases::WAITING_POWER_ON_ANSWER
                             : ProtocolPhases::WAITING_POWER_OFF_ANSWER);
      }
      break;
    case ProtocolPhases::WAITING_FIRST_STATUS_ANSWER:
    case ProtocolPhases::WAITING_STATUS_ANSWER:
    case ProtocolPhases::WAITING_CONTROL_ANSWER:
    case ProtocolPhases::WAITING_POWER_ON_ANSWER:
    case ProtocolPhases::WAITING_POWER_OFF_ANSWER:
      break;
    case ProtocolPhases::IDLE: {
      if (this->forced_request_status_ || this->is_status_request_interval_exceeded_(now)) {
        this->set_phase_(ProtocolPhases::SENDING_STATUS_REQUEST);
        this->forced_request_status_ = false;
      }
    } break;
    default:
      // Shouldn't get here
      ESP_LOGE(TAG, "Wrong protocol handler state: %d, resetting communication", (int) this->protocol_phase_);
      this->set_phase_(ProtocolPhases::SENDING_FIRST_STATUS_REQUEST);
      break;
  }
}

haier_protocol::HaierMessage Smartair2Climate::get_control_message() {
  uint8_t control_out_buffer[sizeof(smartair2_protocol::HaierPacketControl)];
  memcpy(control_out_buffer, this->last_status_message_.get(), sizeof(smartair2_protocol::HaierPacketControl));
  smartair2_protocol::HaierPacketControl *out_data = (smartair2_protocol::HaierPacketControl *) control_out_buffer;
  out_data->cntrl = 0;
  if (this->hvac_settings_.valid) {
    HvacSettings climate_control;
    climate_control = this->hvac_settings_;
    if (climate_control.mode.has_value()) {
      switch (climate_control.mode.value()) {
        case CLIMATE_MODE_OFF:
          out_data->ac_power = 0;
          break;

        case CLIMATE_MODE_AUTO:
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
      switch (climate_control.swing_mode.value()) {
        case CLIMATE_SWING_OFF:
          out_data->use_swing_bits = 0;
          out_data->swing_both = 0;
          break;
        case CLIMATE_SWING_VERTICAL:
          out_data->swing_both = 0;
          out_data->vertical_swing = 1;
          out_data->horizontal_swing = 0;
          break;
        case CLIMATE_SWING_HORIZONTAL:
          out_data->swing_both = 0;
          out_data->vertical_swing = 0;
          out_data->horizontal_swing = 1;
          break;
        case CLIMATE_SWING_BOTH:
          out_data->swing_both = 1;
          out_data->use_swing_bits = 0;
          out_data->vertical_swing = 0;
          out_data->horizontal_swing = 0;
          break;
      }
    }
    if (climate_control.target_temperature.has_value()) {
      out_data->set_point =
          climate_control.target_temperature.value() - 16;  // set the temperature at our offset, subtract 16.
    }
    if (out_data->ac_power == 0) {
      // If AC is off - no presets alowed
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
  return haier_protocol::HaierMessage((uint8_t) smartair2_protocol::FrameType::CONTROL, 0x4D5F, control_out_buffer,
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
    this->target_temperature = packet.control.set_point + 16.0f;
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
        // Somtimes AC reports in fan only mode that fan speed is auto
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
          this->set_force_send_control_(true);
        } else {
          this->display_status_ = disp_status;
        }
      }
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
          this->mode = CLIMATE_MODE_AUTO;
          break;
      }
    }
    should_publish = should_publish || (old_mode != this->mode);
  }
  {
    // Health mode
    bool old_health_mode = this->health_mode_;
    this->health_mode_ = packet.control.health_mode == 1;
    should_publish = should_publish || (old_health_mode != this->health_mode_);
  }
  {
    // Swing mode
    ClimateSwingMode old_swing_mode = this->swing_mode;
    if (packet.control.swing_both == 0) {
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
                  "Horizontal Swing Status = 0x%X", packet.control.horizontal_swing);
  esp_log_printf_((should_publish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__,
                  "Vertical Swing Status = 0x%X", packet.control.vertical_swing);
  esp_log_printf_((should_publish ? ESPHOME_LOG_LEVEL_INFO : ESPHOME_LOG_LEVEL_DEBUG), TAG, __LINE__,
                  "Set Point Status = 0x%X", packet.control.set_point);
  return haier_protocol::HandlerError::HANDLER_OK;
}

bool Smartair2Climate::is_message_invalid(uint8_t message_type) {
  return message_type == (uint8_t) smartair2_protocol::FrameType::INVALID;
}

}  // namespace haier
}  // namespace esphome
