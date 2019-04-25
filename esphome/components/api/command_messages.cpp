#include "command_messages.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

#ifdef USE_COVER
bool CoverCommandRequest::decode_varint(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 2:
      // bool has_legacy_command = 2;
      this->has_legacy_command_ = value;
      return true;
    case 3:
      // enum LegacyCoverCommand {
      //   OPEN = 0;
      //   CLOSE = 1;
      //   STOP = 2;
      // }
      // LegacyCoverCommand legacy_command_ = 3;
      this->legacy_command_ = static_cast<LegacyCoverCommand>(value);
      return true;
    case 4:
      // bool has_position = 4;
      this->has_position_ = value;
      return true;
    case 6:
      // bool has_tilt = 6;
      this->has_tilt_ = value;
      return true;
    case 8:
      // bool stop = 8;
      this->stop_ = value;
    default:
      return false;
  }
}
bool CoverCommandRequest::decode_32bit(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 1:
      // fixed32 key = 1;
      this->key_ = value;
      return true;
    case 5:
      // float position = 5;
      this->position_ = as_float(value);
      return true;
    case 7:
      // float tilt = 7;
      this->tilt_ = as_float(value);
      return true;
    default:
      return false;
  }
}
APIMessageType CoverCommandRequest::message_type() const { return APIMessageType ::COVER_COMMAND_REQUEST; }
uint32_t CoverCommandRequest::get_key() const { return this->key_; }
optional<LegacyCoverCommand> CoverCommandRequest::get_legacy_command() const {
  if (!this->has_legacy_command_)
    return {};
  return this->legacy_command_;
}
optional<float> CoverCommandRequest::get_position() const {
  if (!this->has_position_)
    return {};
  return this->position_;
}
optional<float> CoverCommandRequest::get_tilt() const {
  if (!this->has_tilt_)
    return {};
  return this->tilt_;
}
#endif

#ifdef USE_FAN
bool FanCommandRequest::decode_varint(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 2:
      // bool has_state = 2;
      this->has_state_ = value;
      return true;
    case 3:
      // bool state = 3;
      this->state_ = value;
      return true;
    case 4:
      // bool has_speed = 4;
      this->has_speed_ = value;
      return true;
    case 5:
      // FanSpeed speed = 5;
      this->speed_ = static_cast<fan::FanSpeed>(value);
      return true;
    case 6:
      // bool has_oscillating = 6;
      this->has_oscillating_ = value;
      return true;
    case 7:
      // bool oscillating = 7;
      this->oscillating_ = value;
      return true;
    default:
      return false;
  }
}
bool FanCommandRequest::decode_32bit(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 1:
      // fixed32 key = 1;
      this->key_ = value;
      return true;
    default:
      return false;
  }
}
APIMessageType FanCommandRequest::message_type() const { return APIMessageType::FAN_COMMAND_REQUEST; }
uint32_t FanCommandRequest::get_key() const { return this->key_; }
optional<bool> FanCommandRequest::get_state() const {
  if (!this->has_state_)
    return {};
  return this->state_;
}
optional<fan::FanSpeed> FanCommandRequest::get_speed() const {
  if (!this->has_speed_)
    return {};
  return this->speed_;
}
optional<bool> FanCommandRequest::get_oscillating() const {
  if (!this->has_oscillating_)
    return {};
  return this->oscillating_;
}
#endif

#ifdef USE_LIGHT
bool LightCommandRequest::decode_varint(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 2:
      // bool has_state = 2;
      this->has_state_ = value;
      return true;
    case 3:
      // bool state = 3;
      this->state_ = value;
      return true;
    case 4:
      // bool has_brightness = 4;
      this->has_brightness_ = value;
      return true;
    case 6:
      // bool has_rgb = 6;
      this->has_rgb_ = value;
      return true;
    case 10:
      // bool has_white = 10;
      this->has_white_ = value;
      return true;
    case 12:
      // bool has_color_temperature = 12;
      this->has_color_temperature_ = value;
      return true;
    case 14:
      // bool has_transition_length = 14;
      this->has_transition_length_ = value;
      return true;
    case 15:
      // uint32 transition_length = 15;
      this->transition_length_ = value;
      return true;
    case 16:
      // bool has_flash_length = 16;
      this->has_flash_length_ = value;
      return true;
    case 17:
      // uint32 flash_length = 17;
      this->flash_length_ = value;
      return true;
    case 18:
      // bool has_effect = 18;
      this->has_effect_ = value;
      return true;
    default:
      return false;
  }
}
bool LightCommandRequest::decode_length_delimited(uint32_t field_id, const uint8_t *value, size_t len) {
  switch (field_id) {
    case 19:
      // string effect = 19;
      this->effect_ = as_string(value, len);
      return true;
    default:
      return false;
  }
}
bool LightCommandRequest::decode_32bit(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 1:
      // fixed32 key = 1;
      this->key_ = value;
      return true;
    case 5:
      // float brightness = 5;
      this->brightness_ = as_float(value);
      return true;
    case 7:
      // float red = 7;
      this->red_ = as_float(value);
      return true;
    case 8:
      // float green = 8;
      this->green_ = as_float(value);
      return true;
    case 9:
      // float blue = 9;
      this->blue_ = as_float(value);
      return true;
    case 11:
      // float white = 11;
      this->white_ = as_float(value);
      return true;
    case 13:
      // float color_temperature = 13;
      this->color_temperature_ = as_float(value);
      return true;
    default:
      return false;
  }
}
APIMessageType LightCommandRequest::message_type() const { return APIMessageType::LIGHT_COMMAND_REQUEST; }
uint32_t LightCommandRequest::get_key() const { return this->key_; }
optional<bool> LightCommandRequest::get_state() const {
  if (!this->has_state_)
    return {};
  return this->state_;
}
optional<float> LightCommandRequest::get_brightness() const {
  if (!this->has_brightness_)
    return {};
  return this->brightness_;
}
optional<float> LightCommandRequest::get_red() const {
  if (!this->has_rgb_)
    return {};
  return this->red_;
}
optional<float> LightCommandRequest::get_green() const {
  if (!this->has_rgb_)
    return {};
  return this->green_;
}
optional<float> LightCommandRequest::get_blue() const {
  if (!this->has_rgb_)
    return {};
  return this->blue_;
}
optional<float> LightCommandRequest::get_white() const {
  if (!this->has_white_)
    return {};
  return this->white_;
}
optional<float> LightCommandRequest::get_color_temperature() const {
  if (!this->has_color_temperature_)
    return {};
  return this->color_temperature_;
}
optional<uint32_t> LightCommandRequest::get_transition_length() const {
  if (!this->has_transition_length_)
    return {};
  return this->transition_length_;
}
optional<uint32_t> LightCommandRequest::get_flash_length() const {
  if (!this->has_flash_length_)
    return {};
  return this->flash_length_;
}
optional<std::string> LightCommandRequest::get_effect() const {
  if (!this->has_effect_)
    return {};
  return this->effect_;
}
#endif

#ifdef USE_SWITCH
bool SwitchCommandRequest::decode_varint(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 2:
      // bool state = 2;
      this->state_ = value;
      return true;
    default:
      return false;
  }
}
bool SwitchCommandRequest::decode_32bit(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 1:
      // fixed32 key = 1;
      this->key_ = value;
      return true;
    default:
      return false;
  }
}
APIMessageType SwitchCommandRequest::message_type() const { return APIMessageType::SWITCH_COMMAND_REQUEST; }
uint32_t SwitchCommandRequest::get_key() const { return this->key_; }
bool SwitchCommandRequest::get_state() const { return this->state_; }
#endif

#ifdef USE_ESP32_CAMERA
bool CameraImageRequest::get_single() const { return this->single_; }
bool CameraImageRequest::get_stream() const { return this->stream_; }
bool CameraImageRequest::decode_varint(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 1:
      // bool single = 1;
      this->single_ = value;
      return true;
    case 2:
      // bool stream = 2;
      this->stream_ = value;
      return true;
    default:
      return false;
  }
}
APIMessageType CameraImageRequest::message_type() const { return APIMessageType::CAMERA_IMAGE_REQUEST; }
#endif

#ifdef USE_CLIMATE
bool ClimateCommandRequest::decode_varint(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 2:
      // bool has_mode = 2;
      this->has_mode_ = value;
      return true;
    case 3:
      // ClimateMode mode = 3;
      this->mode_ = static_cast<climate::ClimateMode>(value);
      return true;
    case 4:
      // bool has_target_temperature = 4;
      this->has_target_temperature_ = value;
      return true;
    case 6:
      // bool has_target_temperature_low = 6;
      this->has_target_temperature_low_ = value;
      return true;
    case 8:
      // bool has_target_temperature_high = 8;
      this->has_target_temperature_high_ = value;
      return true;
    case 10:
      // bool has_away = 10;
      this->has_away_ = value;
      return true;
    case 11:
      // bool away = 11;
      this->away_ = value;
      return true;
    default:
      return false;
  }
}
bool ClimateCommandRequest::decode_32bit(uint32_t field_id, uint32_t value) {
  switch (field_id) {
    case 1:
      // fixed32 key = 1;
      this->key_ = value;
      return true;
    case 5:
      // float target_temperature = 5;
      this->target_temperature_ = as_float(value);
      return true;
    case 7:
      // float target_temperature_low = 7;
      this->target_temperature_low_ = as_float(value);
      return true;
    case 9:
      // float target_temperature_high = 9;
      this->target_temperature_high_ = as_float(value);
      return true;
    default:
      return false;
  }
}
APIMessageType ClimateCommandRequest::message_type() const { return APIMessageType::CLIMATE_COMMAND_REQUEST; }
uint32_t ClimateCommandRequest::get_key() const { return this->key_; }
optional<climate::ClimateMode> ClimateCommandRequest::get_mode() const {
  if (!this->has_mode_)
    return {};
  return this->mode_;
}
optional<float> ClimateCommandRequest::get_target_temperature() const {
  if (!this->has_target_temperature_)
    return {};
  return this->target_temperature_;
}
optional<float> ClimateCommandRequest::get_target_temperature_low() const {
  if (!this->has_target_temperature_low_)
    return {};
  return this->target_temperature_low_;
}
optional<float> ClimateCommandRequest::get_target_temperature_high() const {
  if (!this->has_target_temperature_high_)
    return {};
  return this->target_temperature_high_;
}
optional<bool> ClimateCommandRequest::get_away() const {
  if (!this->has_away_)
    return {};
  return this->away_;
}
#endif

}  // namespace api
}  // namespace esphome
