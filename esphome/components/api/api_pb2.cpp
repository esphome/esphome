// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#include "api_pb2.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>

namespace esphome::api {

bool HelloRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->api_version_major = value.as_uint32();
      break;
    case 3:
      this->api_version_minor = value.as_uint32();
      break;
    default:
      return false;
  }
  return true;
}
bool HelloRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      // Use raw data directly to avoid allocation
      this->client_info = value.data();
      this->client_info_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
void HelloResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->api_version_major);
  buffer.encode_uint32(2, this->api_version_minor);
  buffer.encode_string(3, this->server_info_ref_);
  buffer.encode_string(4, this->name_ref_);
}
void HelloResponse::calculate_size(ProtoSize &size) const {
  size.add_uint32(1, this->api_version_major);
  size.add_uint32(1, this->api_version_minor);
  size.add_length(1, this->server_info_ref_.size());
  size.add_length(1, this->name_ref_.size());
}
#ifdef USE_API_PASSWORD
bool AuthenticationRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      // Use raw data directly to avoid allocation
      this->password = value.data();
      this->password_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
void AuthenticationResponse::encode(ProtoWriteBuffer buffer) const { buffer.encode_bool(1, this->invalid_password); }
void AuthenticationResponse::calculate_size(ProtoSize &size) const { size.add_bool(1, this->invalid_password); }
#endif
#ifdef USE_AREAS
void AreaInfo::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->area_id);
  buffer.encode_string(2, this->name_ref_);
}
void AreaInfo::calculate_size(ProtoSize &size) const {
  size.add_uint32(1, this->area_id);
  size.add_length(1, this->name_ref_.size());
}
#endif
#ifdef USE_DEVICES
void DeviceInfo::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->device_id);
  buffer.encode_string(2, this->name_ref_);
  buffer.encode_uint32(3, this->area_id);
}
void DeviceInfo::calculate_size(ProtoSize &size) const {
  size.add_uint32(1, this->device_id);
  size.add_length(1, this->name_ref_.size());
  size.add_uint32(1, this->area_id);
}
#endif
void DeviceInfoResponse::encode(ProtoWriteBuffer buffer) const {
#ifdef USE_API_PASSWORD
  buffer.encode_bool(1, this->uses_password);
#endif
  buffer.encode_string(2, this->name_ref_);
  buffer.encode_string(3, this->mac_address_ref_);
  buffer.encode_string(4, this->esphome_version_ref_);
  buffer.encode_string(5, this->compilation_time_ref_);
  buffer.encode_string(6, this->model_ref_);
#ifdef USE_DEEP_SLEEP
  buffer.encode_bool(7, this->has_deep_sleep);
#endif
#ifdef ESPHOME_PROJECT_NAME
  buffer.encode_string(8, this->project_name_ref_);
#endif
#ifdef ESPHOME_PROJECT_NAME
  buffer.encode_string(9, this->project_version_ref_);
#endif
#ifdef USE_WEBSERVER
  buffer.encode_uint32(10, this->webserver_port);
#endif
#ifdef USE_BLUETOOTH_PROXY
  buffer.encode_uint32(15, this->bluetooth_proxy_feature_flags);
#endif
  buffer.encode_string(12, this->manufacturer_ref_);
  buffer.encode_string(13, this->friendly_name_ref_);
#ifdef USE_VOICE_ASSISTANT
  buffer.encode_uint32(17, this->voice_assistant_feature_flags);
#endif
#ifdef USE_AREAS
  buffer.encode_string(16, this->suggested_area_ref_);
#endif
#ifdef USE_BLUETOOTH_PROXY
  buffer.encode_string(18, this->bluetooth_mac_address_ref_);
#endif
#ifdef USE_API_NOISE
  buffer.encode_bool(19, this->api_encryption_supported);
#endif
#ifdef USE_DEVICES
  for (const auto &it : this->devices) {
    buffer.encode_message(20, it, true);
  }
#endif
#ifdef USE_AREAS
  for (const auto &it : this->areas) {
    buffer.encode_message(21, it, true);
  }
#endif
#ifdef USE_AREAS
  buffer.encode_message(22, this->area);
#endif
#ifdef USE_ZWAVE_PROXY
  buffer.encode_uint32(23, this->zwave_proxy_feature_flags);
#endif
#ifdef USE_ZWAVE_PROXY
  buffer.encode_uint32(24, this->zwave_home_id);
#endif
}
void DeviceInfoResponse::calculate_size(ProtoSize &size) const {
#ifdef USE_API_PASSWORD
  size.add_bool(1, this->uses_password);
#endif
  size.add_length(1, this->name_ref_.size());
  size.add_length(1, this->mac_address_ref_.size());
  size.add_length(1, this->esphome_version_ref_.size());
  size.add_length(1, this->compilation_time_ref_.size());
  size.add_length(1, this->model_ref_.size());
#ifdef USE_DEEP_SLEEP
  size.add_bool(1, this->has_deep_sleep);
#endif
#ifdef ESPHOME_PROJECT_NAME
  size.add_length(1, this->project_name_ref_.size());
#endif
#ifdef ESPHOME_PROJECT_NAME
  size.add_length(1, this->project_version_ref_.size());
#endif
#ifdef USE_WEBSERVER
  size.add_uint32(1, this->webserver_port);
#endif
#ifdef USE_BLUETOOTH_PROXY
  size.add_uint32(1, this->bluetooth_proxy_feature_flags);
#endif
  size.add_length(1, this->manufacturer_ref_.size());
  size.add_length(1, this->friendly_name_ref_.size());
#ifdef USE_VOICE_ASSISTANT
  size.add_uint32(2, this->voice_assistant_feature_flags);
#endif
#ifdef USE_AREAS
  size.add_length(2, this->suggested_area_ref_.size());
#endif
#ifdef USE_BLUETOOTH_PROXY
  size.add_length(2, this->bluetooth_mac_address_ref_.size());
#endif
#ifdef USE_API_NOISE
  size.add_bool(2, this->api_encryption_supported);
#endif
#ifdef USE_DEVICES
  for (const auto &it : this->devices) {
    size.add_message_object_force(2, it);
  }
#endif
#ifdef USE_AREAS
  for (const auto &it : this->areas) {
    size.add_message_object_force(2, it);
  }
#endif
#ifdef USE_AREAS
  size.add_message_object(2, this->area);
#endif
#ifdef USE_ZWAVE_PROXY
  size.add_uint32(2, this->zwave_proxy_feature_flags);
#endif
#ifdef USE_ZWAVE_PROXY
  size.add_uint32(2, this->zwave_home_id);
#endif
}
#ifdef USE_BINARY_SENSOR
void ListEntitiesBinarySensorResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
  buffer.encode_string(5, this->device_class_ref_);
  buffer.encode_bool(6, this->is_status_binary_sensor);
  buffer.encode_bool(7, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(8, this->icon_ref_);
#endif
  buffer.encode_uint32(9, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  buffer.encode_uint32(10, this->device_id);
#endif
}
void ListEntitiesBinarySensorResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
  size.add_length(1, this->device_class_ref_.size());
  size.add_bool(1, this->is_status_binary_sensor);
  size.add_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void BinarySensorStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
  buffer.encode_bool(3, this->missing_state);
#ifdef USE_DEVICES
  buffer.encode_uint32(4, this->device_id);
#endif
}
void BinarySensorStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_bool(1, this->state);
  size.add_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
#endif
#ifdef USE_COVER
void ListEntitiesCoverResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
  buffer.encode_bool(5, this->assumed_state);
  buffer.encode_bool(6, this->supports_position);
  buffer.encode_bool(7, this->supports_tilt);
  buffer.encode_string(8, this->device_class_ref_);
  buffer.encode_bool(9, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(10, this->icon_ref_);
#endif
  buffer.encode_uint32(11, static_cast<uint32_t>(this->entity_category));
  buffer.encode_bool(12, this->supports_stop);
#ifdef USE_DEVICES
  buffer.encode_uint32(13, this->device_id);
#endif
}
void ListEntitiesCoverResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
  size.add_bool(1, this->assumed_state);
  size.add_bool(1, this->supports_position);
  size.add_bool(1, this->supports_tilt);
  size.add_length(1, this->device_class_ref_.size());
  size.add_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_bool(1, this->supports_stop);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void CoverStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_float(3, this->position);
  buffer.encode_float(4, this->tilt);
  buffer.encode_uint32(5, static_cast<uint32_t>(this->current_operation));
#ifdef USE_DEVICES
  buffer.encode_uint32(6, this->device_id);
#endif
}
void CoverStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_float(1, this->position);
  size.add_float(1, this->tilt);
  size.add_uint32(1, static_cast<uint32_t>(this->current_operation));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool CoverCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 4:
      this->has_position = value.as_bool();
      break;
    case 6:
      this->has_tilt = value.as_bool();
      break;
    case 8:
      this->stop = value.as_bool();
      break;
#ifdef USE_DEVICES
    case 9:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool CoverCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    case 5:
      this->position = value.as_float();
      break;
    case 7:
      this->tilt = value.as_float();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_FAN
void ListEntitiesFanResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
  buffer.encode_bool(5, this->supports_oscillation);
  buffer.encode_bool(6, this->supports_speed);
  buffer.encode_bool(7, this->supports_direction);
  buffer.encode_int32(8, this->supported_speed_count);
  buffer.encode_bool(9, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(10, this->icon_ref_);
#endif
  buffer.encode_uint32(11, static_cast<uint32_t>(this->entity_category));
  for (const char *it : *this->supported_preset_modes) {
    buffer.encode_string(12, it, strlen(it), true);
  }
#ifdef USE_DEVICES
  buffer.encode_uint32(13, this->device_id);
#endif
}
void ListEntitiesFanResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
  size.add_bool(1, this->supports_oscillation);
  size.add_bool(1, this->supports_speed);
  size.add_bool(1, this->supports_direction);
  size.add_int32(1, this->supported_speed_count);
  size.add_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  if (!this->supported_preset_modes->empty()) {
    for (const char *it : *this->supported_preset_modes) {
      size.add_length_force(1, strlen(it));
    }
  }
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void FanStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
  buffer.encode_bool(3, this->oscillating);
  buffer.encode_uint32(5, static_cast<uint32_t>(this->direction));
  buffer.encode_int32(6, this->speed_level);
  buffer.encode_string(7, this->preset_mode_ref_);
#ifdef USE_DEVICES
  buffer.encode_uint32(8, this->device_id);
#endif
}
void FanStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_bool(1, this->state);
  size.add_bool(1, this->oscillating);
  size.add_uint32(1, static_cast<uint32_t>(this->direction));
  size.add_int32(1, this->speed_level);
  size.add_length(1, this->preset_mode_ref_.size());
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool FanCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->has_state = value.as_bool();
      break;
    case 3:
      this->state = value.as_bool();
      break;
    case 6:
      this->has_oscillating = value.as_bool();
      break;
    case 7:
      this->oscillating = value.as_bool();
      break;
    case 8:
      this->has_direction = value.as_bool();
      break;
    case 9:
      this->direction = static_cast<enums::FanDirection>(value.as_uint32());
      break;
    case 10:
      this->has_speed_level = value.as_bool();
      break;
    case 11:
      this->speed_level = value.as_int32();
      break;
    case 12:
      this->has_preset_mode = value.as_bool();
      break;
#ifdef USE_DEVICES
    case 14:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool FanCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 13:
      this->preset_mode = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
bool FanCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_LIGHT
void ListEntitiesLightResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
  for (const auto &it : *this->supported_color_modes) {
    buffer.encode_uint32(12, static_cast<uint32_t>(it), true);
  }
  buffer.encode_float(9, this->min_mireds);
  buffer.encode_float(10, this->max_mireds);
  for (const char *it : *this->effects) {
    buffer.encode_string(11, it, strlen(it), true);
  }
  buffer.encode_bool(13, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(14, this->icon_ref_);
#endif
  buffer.encode_uint32(15, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  buffer.encode_uint32(16, this->device_id);
#endif
}
void ListEntitiesLightResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
  if (!this->supported_color_modes->empty()) {
    for (const auto &it : *this->supported_color_modes) {
      size.add_uint32_force(1, static_cast<uint32_t>(it));
    }
  }
  size.add_float(1, this->min_mireds);
  size.add_float(1, this->max_mireds);
  if (!this->effects->empty()) {
    for (const char *it : *this->effects) {
      size.add_length_force(1, strlen(it));
    }
  }
  size.add_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size.add_uint32(2, this->device_id);
#endif
}
void LightStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
  buffer.encode_float(3, this->brightness);
  buffer.encode_uint32(11, static_cast<uint32_t>(this->color_mode));
  buffer.encode_float(10, this->color_brightness);
  buffer.encode_float(4, this->red);
  buffer.encode_float(5, this->green);
  buffer.encode_float(6, this->blue);
  buffer.encode_float(7, this->white);
  buffer.encode_float(8, this->color_temperature);
  buffer.encode_float(12, this->cold_white);
  buffer.encode_float(13, this->warm_white);
  buffer.encode_string(9, this->effect_ref_);
#ifdef USE_DEVICES
  buffer.encode_uint32(14, this->device_id);
#endif
}
void LightStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_bool(1, this->state);
  size.add_float(1, this->brightness);
  size.add_uint32(1, static_cast<uint32_t>(this->color_mode));
  size.add_float(1, this->color_brightness);
  size.add_float(1, this->red);
  size.add_float(1, this->green);
  size.add_float(1, this->blue);
  size.add_float(1, this->white);
  size.add_float(1, this->color_temperature);
  size.add_float(1, this->cold_white);
  size.add_float(1, this->warm_white);
  size.add_length(1, this->effect_ref_.size());
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool LightCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->has_state = value.as_bool();
      break;
    case 3:
      this->state = value.as_bool();
      break;
    case 4:
      this->has_brightness = value.as_bool();
      break;
    case 22:
      this->has_color_mode = value.as_bool();
      break;
    case 23:
      this->color_mode = static_cast<enums::ColorMode>(value.as_uint32());
      break;
    case 20:
      this->has_color_brightness = value.as_bool();
      break;
    case 6:
      this->has_rgb = value.as_bool();
      break;
    case 10:
      this->has_white = value.as_bool();
      break;
    case 12:
      this->has_color_temperature = value.as_bool();
      break;
    case 24:
      this->has_cold_white = value.as_bool();
      break;
    case 26:
      this->has_warm_white = value.as_bool();
      break;
    case 14:
      this->has_transition_length = value.as_bool();
      break;
    case 15:
      this->transition_length = value.as_uint32();
      break;
    case 16:
      this->has_flash_length = value.as_bool();
      break;
    case 17:
      this->flash_length = value.as_uint32();
      break;
    case 18:
      this->has_effect = value.as_bool();
      break;
#ifdef USE_DEVICES
    case 28:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool LightCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 19: {
      // Use raw data directly to avoid allocation
      this->effect = value.data();
      this->effect_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
bool LightCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    case 5:
      this->brightness = value.as_float();
      break;
    case 21:
      this->color_brightness = value.as_float();
      break;
    case 7:
      this->red = value.as_float();
      break;
    case 8:
      this->green = value.as_float();
      break;
    case 9:
      this->blue = value.as_float();
      break;
    case 11:
      this->white = value.as_float();
      break;
    case 13:
      this->color_temperature = value.as_float();
      break;
    case 25:
      this->cold_white = value.as_float();
      break;
    case 27:
      this->warm_white = value.as_float();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_SENSOR
void ListEntitiesSensorResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_string(6, this->unit_of_measurement_ref_);
  buffer.encode_int32(7, this->accuracy_decimals);
  buffer.encode_bool(8, this->force_update);
  buffer.encode_string(9, this->device_class_ref_);
  buffer.encode_uint32(10, static_cast<uint32_t>(this->state_class));
  buffer.encode_bool(12, this->disabled_by_default);
  buffer.encode_uint32(13, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  buffer.encode_uint32(14, this->device_id);
#endif
}
void ListEntitiesSensorResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_length(1, this->unit_of_measurement_ref_.size());
  size.add_int32(1, this->accuracy_decimals);
  size.add_bool(1, this->force_update);
  size.add_length(1, this->device_class_ref_.size());
  size.add_uint32(1, static_cast<uint32_t>(this->state_class));
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void SensorStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_float(2, this->state);
  buffer.encode_bool(3, this->missing_state);
#ifdef USE_DEVICES
  buffer.encode_uint32(4, this->device_id);
#endif
}
void SensorStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_float(1, this->state);
  size.add_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
#endif
#ifdef USE_SWITCH
void ListEntitiesSwitchResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->assumed_state);
  buffer.encode_bool(7, this->disabled_by_default);
  buffer.encode_uint32(8, static_cast<uint32_t>(this->entity_category));
  buffer.encode_string(9, this->device_class_ref_);
#ifdef USE_DEVICES
  buffer.encode_uint32(10, this->device_id);
#endif
}
void ListEntitiesSwitchResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->assumed_state);
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_length(1, this->device_class_ref_.size());
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void SwitchStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
#ifdef USE_DEVICES
  buffer.encode_uint32(3, this->device_id);
#endif
}
void SwitchStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_bool(1, this->state);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool SwitchCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->state = value.as_bool();
      break;
#ifdef USE_DEVICES
    case 3:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool SwitchCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_TEXT_SENSOR
void ListEntitiesTextSensorResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
  buffer.encode_string(8, this->device_class_ref_);
#ifdef USE_DEVICES
  buffer.encode_uint32(9, this->device_id);
#endif
}
void ListEntitiesTextSensorResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_length(1, this->device_class_ref_.size());
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void TextSensorStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_string(2, this->state_ref_);
  buffer.encode_bool(3, this->missing_state);
#ifdef USE_DEVICES
  buffer.encode_uint32(4, this->device_id);
#endif
}
void TextSensorStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_length(1, this->state_ref_.size());
  size.add_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
#endif
bool SubscribeLogsRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->level = static_cast<enums::LogLevel>(value.as_uint32());
      break;
    case 2:
      this->dump_config = value.as_bool();
      break;
    default:
      return false;
  }
  return true;
}
void SubscribeLogsResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, static_cast<uint32_t>(this->level));
  buffer.encode_bytes(3, this->message_ptr_, this->message_len_);
}
void SubscribeLogsResponse::calculate_size(ProtoSize &size) const {
  size.add_uint32(1, static_cast<uint32_t>(this->level));
  size.add_length(1, this->message_len_);
}
#ifdef USE_API_NOISE
bool NoiseEncryptionSetKeyRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1:
      this->key = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
void NoiseEncryptionSetKeyResponse::encode(ProtoWriteBuffer buffer) const { buffer.encode_bool(1, this->success); }
void NoiseEncryptionSetKeyResponse::calculate_size(ProtoSize &size) const { size.add_bool(1, this->success); }
#endif
#ifdef USE_API_HOMEASSISTANT_SERVICES
void HomeassistantServiceMap::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->key_ref_);
  buffer.encode_string(2, this->value);
}
void HomeassistantServiceMap::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->key_ref_.size());
  size.add_length(1, this->value.size());
}
void HomeassistantActionRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->service_ref_);
  for (auto &it : this->data) {
    buffer.encode_message(2, it, true);
  }
  for (auto &it : this->data_template) {
    buffer.encode_message(3, it, true);
  }
  for (auto &it : this->variables) {
    buffer.encode_message(4, it, true);
  }
  buffer.encode_bool(5, this->is_event);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  buffer.encode_uint32(6, this->call_id);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  buffer.encode_bool(7, this->wants_response);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  buffer.encode_string(8, this->response_template);
#endif
}
void HomeassistantActionRequest::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->service_ref_.size());
  size.add_repeated_message(1, this->data);
  size.add_repeated_message(1, this->data_template);
  size.add_repeated_message(1, this->variables);
  size.add_bool(1, this->is_event);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  size.add_uint32(1, this->call_id);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  size.add_bool(1, this->wants_response);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  size.add_length(1, this->response_template.size());
#endif
}
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
bool HomeassistantActionResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->call_id = value.as_uint32();
      break;
    case 2:
      this->success = value.as_bool();
      break;
    default:
      return false;
  }
  return true;
}
bool HomeassistantActionResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3:
      this->error_message = value.as_string();
      break;
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
    case 4: {
      // Use raw data directly to avoid allocation
      this->response_data = value.data();
      this->response_data_len = value.size();
      break;
    }
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
void SubscribeHomeAssistantStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->entity_id_ref_);
  buffer.encode_string(2, this->attribute_ref_);
  buffer.encode_bool(3, this->once);
}
void SubscribeHomeAssistantStateResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->entity_id_ref_.size());
  size.add_length(1, this->attribute_ref_.size());
  size.add_bool(1, this->once);
}
bool HomeAssistantStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1:
      this->entity_id = value.as_string();
      break;
    case 2:
      this->state = value.as_string();
      break;
    case 3:
      this->attribute = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
#endif
bool GetTimeResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      // Use raw data directly to avoid allocation
      this->timezone = value.data();
      this->timezone_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
bool GetTimeResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->epoch_seconds = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#ifdef USE_API_USER_DEFINED_ACTIONS
void ListEntitiesServicesArgument::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->name_ref_);
  buffer.encode_uint32(2, static_cast<uint32_t>(this->type));
}
void ListEntitiesServicesArgument::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->name_ref_.size());
  size.add_uint32(1, static_cast<uint32_t>(this->type));
}
void ListEntitiesServicesResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->name_ref_);
  buffer.encode_fixed32(2, this->key);
  for (auto &it : this->args) {
    buffer.encode_message(3, it, true);
  }
  buffer.encode_uint32(4, static_cast<uint32_t>(this->supports_response));
}
void ListEntitiesServicesResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->name_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_repeated_message(1, this->args);
  size.add_uint32(1, static_cast<uint32_t>(this->supports_response));
}
bool ExecuteServiceArgument::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->bool_ = value.as_bool();
      break;
    case 2:
      this->legacy_int = value.as_int32();
      break;
    case 5:
      this->int_ = value.as_sint32();
      break;
    case 6:
      this->bool_array.push_back(value.as_bool());
      break;
    case 7:
      this->int_array.push_back(value.as_sint32());
      break;
    default:
      return false;
  }
  return true;
}
bool ExecuteServiceArgument::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 4:
      this->string_ = value.as_string();
      break;
    case 9:
      this->string_array.push_back(value.as_string());
      break;
    default:
      return false;
  }
  return true;
}
bool ExecuteServiceArgument::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 3:
      this->float_ = value.as_float();
      break;
    case 8:
      this->float_array.push_back(value.as_float());
      break;
    default:
      return false;
  }
  return true;
}
void ExecuteServiceArgument::decode(const uint8_t *buffer, size_t length) {
  uint32_t count_bool_array = ProtoDecodableMessage::count_repeated_field(buffer, length, 6);
  this->bool_array.init(count_bool_array);
  uint32_t count_int_array = ProtoDecodableMessage::count_repeated_field(buffer, length, 7);
  this->int_array.init(count_int_array);
  uint32_t count_float_array = ProtoDecodableMessage::count_repeated_field(buffer, length, 8);
  this->float_array.init(count_float_array);
  uint32_t count_string_array = ProtoDecodableMessage::count_repeated_field(buffer, length, 9);
  this->string_array.init(count_string_array);
  ProtoDecodableMessage::decode(buffer, length);
}
bool ExecuteServiceRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
    case 3:
      this->call_id = value.as_uint32();
      break;
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
    case 4:
      this->return_response = value.as_bool();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool ExecuteServiceRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2:
      this->args.emplace_back();
      value.decode_to_message(this->args.back());
      break;
    default:
      return false;
  }
  return true;
}
bool ExecuteServiceRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
void ExecuteServiceRequest::decode(const uint8_t *buffer, size_t length) {
  uint32_t count_args = ProtoDecodableMessage::count_repeated_field(buffer, length, 2);
  this->args.init(count_args);
  ProtoDecodableMessage::decode(buffer, length);
}
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
void ExecuteServiceResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->call_id);
  buffer.encode_bool(2, this->success);
  buffer.encode_string(3, this->error_message_ref_);
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  buffer.encode_bytes(4, this->response_data, this->response_data_len);
#endif
}
void ExecuteServiceResponse::calculate_size(ProtoSize &size) const {
  size.add_uint32(1, this->call_id);
  size.add_bool(1, this->success);
  size.add_length(1, this->error_message_ref_.size());
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  size.add_length(4, this->response_data_len);
#endif
}
#endif
#ifdef USE_CAMERA
void ListEntitiesCameraResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
  buffer.encode_bool(5, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(6, this->icon_ref_);
#endif
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  buffer.encode_uint32(8, this->device_id);
#endif
}
void ListEntitiesCameraResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
  size.add_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void CameraImageResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bytes(2, this->data_ptr_, this->data_len_);
  buffer.encode_bool(3, this->done);
#ifdef USE_DEVICES
  buffer.encode_uint32(4, this->device_id);
#endif
}
void CameraImageResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_length(1, this->data_len_);
  size.add_bool(1, this->done);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool CameraImageRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->single = value.as_bool();
      break;
    case 2:
      this->stream = value.as_bool();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_CLIMATE
void ListEntitiesClimateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
  buffer.encode_bool(5, this->supports_current_temperature);
  buffer.encode_bool(6, this->supports_two_point_target_temperature);
  for (const auto &it : *this->supported_modes) {
    buffer.encode_uint32(7, static_cast<uint32_t>(it), true);
  }
  buffer.encode_float(8, this->visual_min_temperature);
  buffer.encode_float(9, this->visual_max_temperature);
  buffer.encode_float(10, this->visual_target_temperature_step);
  buffer.encode_bool(12, this->supports_action);
  for (const auto &it : *this->supported_fan_modes) {
    buffer.encode_uint32(13, static_cast<uint32_t>(it), true);
  }
  for (const auto &it : *this->supported_swing_modes) {
    buffer.encode_uint32(14, static_cast<uint32_t>(it), true);
  }
  for (const char *it : *this->supported_custom_fan_modes) {
    buffer.encode_string(15, it, strlen(it), true);
  }
  for (const auto &it : *this->supported_presets) {
    buffer.encode_uint32(16, static_cast<uint32_t>(it), true);
  }
  for (const char *it : *this->supported_custom_presets) {
    buffer.encode_string(17, it, strlen(it), true);
  }
  buffer.encode_bool(18, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(19, this->icon_ref_);
#endif
  buffer.encode_uint32(20, static_cast<uint32_t>(this->entity_category));
  buffer.encode_float(21, this->visual_current_temperature_step);
  buffer.encode_bool(22, this->supports_current_humidity);
  buffer.encode_bool(23, this->supports_target_humidity);
  buffer.encode_float(24, this->visual_min_humidity);
  buffer.encode_float(25, this->visual_max_humidity);
#ifdef USE_DEVICES
  buffer.encode_uint32(26, this->device_id);
#endif
  buffer.encode_uint32(27, this->feature_flags);
}
void ListEntitiesClimateResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
  size.add_bool(1, this->supports_current_temperature);
  size.add_bool(1, this->supports_two_point_target_temperature);
  if (!this->supported_modes->empty()) {
    for (const auto &it : *this->supported_modes) {
      size.add_uint32_force(1, static_cast<uint32_t>(it));
    }
  }
  size.add_float(1, this->visual_min_temperature);
  size.add_float(1, this->visual_max_temperature);
  size.add_float(1, this->visual_target_temperature_step);
  size.add_bool(1, this->supports_action);
  if (!this->supported_fan_modes->empty()) {
    for (const auto &it : *this->supported_fan_modes) {
      size.add_uint32_force(1, static_cast<uint32_t>(it));
    }
  }
  if (!this->supported_swing_modes->empty()) {
    for (const auto &it : *this->supported_swing_modes) {
      size.add_uint32_force(1, static_cast<uint32_t>(it));
    }
  }
  if (!this->supported_custom_fan_modes->empty()) {
    for (const char *it : *this->supported_custom_fan_modes) {
      size.add_length_force(1, strlen(it));
    }
  }
  if (!this->supported_presets->empty()) {
    for (const auto &it : *this->supported_presets) {
      size.add_uint32_force(2, static_cast<uint32_t>(it));
    }
  }
  if (!this->supported_custom_presets->empty()) {
    for (const char *it : *this->supported_custom_presets) {
      size.add_length_force(2, strlen(it));
    }
  }
  size.add_bool(2, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size.add_length(2, this->icon_ref_.size());
#endif
  size.add_uint32(2, static_cast<uint32_t>(this->entity_category));
  size.add_float(2, this->visual_current_temperature_step);
  size.add_bool(2, this->supports_current_humidity);
  size.add_bool(2, this->supports_target_humidity);
  size.add_float(2, this->visual_min_humidity);
  size.add_float(2, this->visual_max_humidity);
#ifdef USE_DEVICES
  size.add_uint32(2, this->device_id);
#endif
  size.add_uint32(2, this->feature_flags);
}
void ClimateStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_uint32(2, static_cast<uint32_t>(this->mode));
  buffer.encode_float(3, this->current_temperature);
  buffer.encode_float(4, this->target_temperature);
  buffer.encode_float(5, this->target_temperature_low);
  buffer.encode_float(6, this->target_temperature_high);
  buffer.encode_uint32(8, static_cast<uint32_t>(this->action));
  buffer.encode_uint32(9, static_cast<uint32_t>(this->fan_mode));
  buffer.encode_uint32(10, static_cast<uint32_t>(this->swing_mode));
  buffer.encode_string(11, this->custom_fan_mode_ref_);
  buffer.encode_uint32(12, static_cast<uint32_t>(this->preset));
  buffer.encode_string(13, this->custom_preset_ref_);
  buffer.encode_float(14, this->current_humidity);
  buffer.encode_float(15, this->target_humidity);
#ifdef USE_DEVICES
  buffer.encode_uint32(16, this->device_id);
#endif
}
void ClimateStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_uint32(1, static_cast<uint32_t>(this->mode));
  size.add_float(1, this->current_temperature);
  size.add_float(1, this->target_temperature);
  size.add_float(1, this->target_temperature_low);
  size.add_float(1, this->target_temperature_high);
  size.add_uint32(1, static_cast<uint32_t>(this->action));
  size.add_uint32(1, static_cast<uint32_t>(this->fan_mode));
  size.add_uint32(1, static_cast<uint32_t>(this->swing_mode));
  size.add_length(1, this->custom_fan_mode_ref_.size());
  size.add_uint32(1, static_cast<uint32_t>(this->preset));
  size.add_length(1, this->custom_preset_ref_.size());
  size.add_float(1, this->current_humidity);
  size.add_float(1, this->target_humidity);
#ifdef USE_DEVICES
  size.add_uint32(2, this->device_id);
#endif
}
bool ClimateCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->has_mode = value.as_bool();
      break;
    case 3:
      this->mode = static_cast<enums::ClimateMode>(value.as_uint32());
      break;
    case 4:
      this->has_target_temperature = value.as_bool();
      break;
    case 6:
      this->has_target_temperature_low = value.as_bool();
      break;
    case 8:
      this->has_target_temperature_high = value.as_bool();
      break;
    case 12:
      this->has_fan_mode = value.as_bool();
      break;
    case 13:
      this->fan_mode = static_cast<enums::ClimateFanMode>(value.as_uint32());
      break;
    case 14:
      this->has_swing_mode = value.as_bool();
      break;
    case 15:
      this->swing_mode = static_cast<enums::ClimateSwingMode>(value.as_uint32());
      break;
    case 16:
      this->has_custom_fan_mode = value.as_bool();
      break;
    case 18:
      this->has_preset = value.as_bool();
      break;
    case 19:
      this->preset = static_cast<enums::ClimatePreset>(value.as_uint32());
      break;
    case 20:
      this->has_custom_preset = value.as_bool();
      break;
    case 22:
      this->has_target_humidity = value.as_bool();
      break;
#ifdef USE_DEVICES
    case 24:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool ClimateCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 17:
      this->custom_fan_mode = value.as_string();
      break;
    case 21:
      this->custom_preset = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
bool ClimateCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    case 5:
      this->target_temperature = value.as_float();
      break;
    case 7:
      this->target_temperature_low = value.as_float();
      break;
    case 9:
      this->target_temperature_high = value.as_float();
      break;
    case 23:
      this->target_humidity = value.as_float();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_NUMBER
void ListEntitiesNumberResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_float(6, this->min_value);
  buffer.encode_float(7, this->max_value);
  buffer.encode_float(8, this->step);
  buffer.encode_bool(9, this->disabled_by_default);
  buffer.encode_uint32(10, static_cast<uint32_t>(this->entity_category));
  buffer.encode_string(11, this->unit_of_measurement_ref_);
  buffer.encode_uint32(12, static_cast<uint32_t>(this->mode));
  buffer.encode_string(13, this->device_class_ref_);
#ifdef USE_DEVICES
  buffer.encode_uint32(14, this->device_id);
#endif
}
void ListEntitiesNumberResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_float(1, this->min_value);
  size.add_float(1, this->max_value);
  size.add_float(1, this->step);
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_length(1, this->unit_of_measurement_ref_.size());
  size.add_uint32(1, static_cast<uint32_t>(this->mode));
  size.add_length(1, this->device_class_ref_.size());
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void NumberStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_float(2, this->state);
  buffer.encode_bool(3, this->missing_state);
#ifdef USE_DEVICES
  buffer.encode_uint32(4, this->device_id);
#endif
}
void NumberStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_float(1, this->state);
  size.add_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool NumberCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
#ifdef USE_DEVICES
    case 3:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool NumberCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    case 2:
      this->state = value.as_float();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_SELECT
void ListEntitiesSelectResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  for (const char *it : *this->options) {
    buffer.encode_string(6, it, strlen(it), true);
  }
  buffer.encode_bool(7, this->disabled_by_default);
  buffer.encode_uint32(8, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  buffer.encode_uint32(9, this->device_id);
#endif
}
void ListEntitiesSelectResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  if (!this->options->empty()) {
    for (const char *it : *this->options) {
      size.add_length_force(1, strlen(it));
    }
  }
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void SelectStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_string(2, this->state_ref_);
  buffer.encode_bool(3, this->missing_state);
#ifdef USE_DEVICES
  buffer.encode_uint32(4, this->device_id);
#endif
}
void SelectStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_length(1, this->state_ref_.size());
  size.add_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool SelectCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
#ifdef USE_DEVICES
    case 3:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool SelectCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      // Use raw data directly to avoid allocation
      this->state = value.data();
      this->state_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
bool SelectCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_SIREN
void ListEntitiesSirenResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  for (auto &it : this->tones) {
    buffer.encode_string(7, it, true);
  }
  buffer.encode_bool(8, this->supports_duration);
  buffer.encode_bool(9, this->supports_volume);
  buffer.encode_uint32(10, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  buffer.encode_uint32(11, this->device_id);
#endif
}
void ListEntitiesSirenResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  if (!this->tones.empty()) {
    for (const auto &it : this->tones) {
      size.add_length_force(1, it.size());
    }
  }
  size.add_bool(1, this->supports_duration);
  size.add_bool(1, this->supports_volume);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void SirenStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
#ifdef USE_DEVICES
  buffer.encode_uint32(3, this->device_id);
#endif
}
void SirenStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_bool(1, this->state);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool SirenCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->has_state = value.as_bool();
      break;
    case 3:
      this->state = value.as_bool();
      break;
    case 4:
      this->has_tone = value.as_bool();
      break;
    case 6:
      this->has_duration = value.as_bool();
      break;
    case 7:
      this->duration = value.as_uint32();
      break;
    case 8:
      this->has_volume = value.as_bool();
      break;
#ifdef USE_DEVICES
    case 10:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool SirenCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 5:
      this->tone = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
bool SirenCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    case 9:
      this->volume = value.as_float();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_LOCK
void ListEntitiesLockResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
  buffer.encode_bool(8, this->assumed_state);
  buffer.encode_bool(9, this->supports_open);
  buffer.encode_bool(10, this->requires_code);
  buffer.encode_string(11, this->code_format_ref_);
#ifdef USE_DEVICES
  buffer.encode_uint32(12, this->device_id);
#endif
}
void ListEntitiesLockResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_bool(1, this->assumed_state);
  size.add_bool(1, this->supports_open);
  size.add_bool(1, this->requires_code);
  size.add_length(1, this->code_format_ref_.size());
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void LockStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_uint32(2, static_cast<uint32_t>(this->state));
#ifdef USE_DEVICES
  buffer.encode_uint32(3, this->device_id);
#endif
}
void LockStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_uint32(1, static_cast<uint32_t>(this->state));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool LockCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->command = static_cast<enums::LockCommand>(value.as_uint32());
      break;
    case 3:
      this->has_code = value.as_bool();
      break;
#ifdef USE_DEVICES
    case 5:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool LockCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 4:
      this->code = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
bool LockCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_BUTTON
void ListEntitiesButtonResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
  buffer.encode_string(8, this->device_class_ref_);
#ifdef USE_DEVICES
  buffer.encode_uint32(9, this->device_id);
#endif
}
void ListEntitiesButtonResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_length(1, this->device_class_ref_.size());
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool ButtonCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
#ifdef USE_DEVICES
    case 2:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool ButtonCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_MEDIA_PLAYER
void MediaPlayerSupportedFormat::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->format_ref_);
  buffer.encode_uint32(2, this->sample_rate);
  buffer.encode_uint32(3, this->num_channels);
  buffer.encode_uint32(4, static_cast<uint32_t>(this->purpose));
  buffer.encode_uint32(5, this->sample_bytes);
}
void MediaPlayerSupportedFormat::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->format_ref_.size());
  size.add_uint32(1, this->sample_rate);
  size.add_uint32(1, this->num_channels);
  size.add_uint32(1, static_cast<uint32_t>(this->purpose));
  size.add_uint32(1, this->sample_bytes);
}
void ListEntitiesMediaPlayerResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
  buffer.encode_bool(8, this->supports_pause);
  for (auto &it : this->supported_formats) {
    buffer.encode_message(9, it, true);
  }
#ifdef USE_DEVICES
  buffer.encode_uint32(10, this->device_id);
#endif
  buffer.encode_uint32(11, this->feature_flags);
}
void ListEntitiesMediaPlayerResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_bool(1, this->supports_pause);
  size.add_repeated_message(1, this->supported_formats);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
  size.add_uint32(1, this->feature_flags);
}
void MediaPlayerStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_uint32(2, static_cast<uint32_t>(this->state));
  buffer.encode_float(3, this->volume);
  buffer.encode_bool(4, this->muted);
#ifdef USE_DEVICES
  buffer.encode_uint32(5, this->device_id);
#endif
}
void MediaPlayerStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_uint32(1, static_cast<uint32_t>(this->state));
  size.add_float(1, this->volume);
  size.add_bool(1, this->muted);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool MediaPlayerCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->has_command = value.as_bool();
      break;
    case 3:
      this->command = static_cast<enums::MediaPlayerCommand>(value.as_uint32());
      break;
    case 4:
      this->has_volume = value.as_bool();
      break;
    case 6:
      this->has_media_url = value.as_bool();
      break;
    case 8:
      this->has_announcement = value.as_bool();
      break;
    case 9:
      this->announcement = value.as_bool();
      break;
#ifdef USE_DEVICES
    case 10:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool MediaPlayerCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 7:
      this->media_url = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
bool MediaPlayerCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    case 5:
      this->volume = value.as_float();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_BLUETOOTH_PROXY
bool SubscribeBluetoothLEAdvertisementsRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->flags = value.as_uint32();
      break;
    default:
      return false;
  }
  return true;
}
void BluetoothLERawAdvertisement::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_sint32(2, this->rssi);
  buffer.encode_uint32(3, this->address_type);
  buffer.encode_bytes(4, this->data, this->data_len);
}
void BluetoothLERawAdvertisement::calculate_size(ProtoSize &size) const {
  size.add_uint64(1, this->address);
  size.add_sint32(1, this->rssi);
  size.add_uint32(1, this->address_type);
  size.add_length(1, this->data_len);
}
void BluetoothLERawAdvertisementsResponse::encode(ProtoWriteBuffer buffer) const {
  for (uint16_t i = 0; i < this->advertisements_len; i++) {
    buffer.encode_message(1, this->advertisements[i], true);
  }
}
void BluetoothLERawAdvertisementsResponse::calculate_size(ProtoSize &size) const {
  for (uint16_t i = 0; i < this->advertisements_len; i++) {
    size.add_message_object_force(1, this->advertisements[i]);
  }
}
bool BluetoothDeviceRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->address = value.as_uint64();
      break;
    case 2:
      this->request_type = static_cast<enums::BluetoothDeviceRequestType>(value.as_uint32());
      break;
    case 3:
      this->has_address_type = value.as_bool();
      break;
    case 4:
      this->address_type = value.as_uint32();
      break;
    default:
      return false;
  }
  return true;
}
void BluetoothDeviceConnectionResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_bool(2, this->connected);
  buffer.encode_uint32(3, this->mtu);
  buffer.encode_int32(4, this->error);
}
void BluetoothDeviceConnectionResponse::calculate_size(ProtoSize &size) const {
  size.add_uint64(1, this->address);
  size.add_bool(1, this->connected);
  size.add_uint32(1, this->mtu);
  size.add_int32(1, this->error);
}
bool BluetoothGATTGetServicesRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->address = value.as_uint64();
      break;
    default:
      return false;
  }
  return true;
}
void BluetoothGATTDescriptor::encode(ProtoWriteBuffer buffer) const {
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    buffer.encode_uint64(1, this->uuid[0], true);
    buffer.encode_uint64(1, this->uuid[1], true);
  }
  buffer.encode_uint32(2, this->handle);
  buffer.encode_uint32(3, this->short_uuid);
}
void BluetoothGATTDescriptor::calculate_size(ProtoSize &size) const {
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    size.add_uint64_force(1, this->uuid[0]);
    size.add_uint64_force(1, this->uuid[1]);
  }
  size.add_uint32(1, this->handle);
  size.add_uint32(1, this->short_uuid);
}
void BluetoothGATTCharacteristic::encode(ProtoWriteBuffer buffer) const {
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    buffer.encode_uint64(1, this->uuid[0], true);
    buffer.encode_uint64(1, this->uuid[1], true);
  }
  buffer.encode_uint32(2, this->handle);
  buffer.encode_uint32(3, this->properties);
  for (auto &it : this->descriptors) {
    buffer.encode_message(4, it, true);
  }
  buffer.encode_uint32(5, this->short_uuid);
}
void BluetoothGATTCharacteristic::calculate_size(ProtoSize &size) const {
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    size.add_uint64_force(1, this->uuid[0]);
    size.add_uint64_force(1, this->uuid[1]);
  }
  size.add_uint32(1, this->handle);
  size.add_uint32(1, this->properties);
  size.add_repeated_message(1, this->descriptors);
  size.add_uint32(1, this->short_uuid);
}
void BluetoothGATTService::encode(ProtoWriteBuffer buffer) const {
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    buffer.encode_uint64(1, this->uuid[0], true);
    buffer.encode_uint64(1, this->uuid[1], true);
  }
  buffer.encode_uint32(2, this->handle);
  for (auto &it : this->characteristics) {
    buffer.encode_message(3, it, true);
  }
  buffer.encode_uint32(4, this->short_uuid);
}
void BluetoothGATTService::calculate_size(ProtoSize &size) const {
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    size.add_uint64_force(1, this->uuid[0]);
    size.add_uint64_force(1, this->uuid[1]);
  }
  size.add_uint32(1, this->handle);
  size.add_repeated_message(1, this->characteristics);
  size.add_uint32(1, this->short_uuid);
}
void BluetoothGATTGetServicesResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  for (auto &it : this->services) {
    buffer.encode_message(2, it, true);
  }
}
void BluetoothGATTGetServicesResponse::calculate_size(ProtoSize &size) const {
  size.add_uint64(1, this->address);
  size.add_repeated_message(1, this->services);
}
void BluetoothGATTGetServicesDoneResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
}
void BluetoothGATTGetServicesDoneResponse::calculate_size(ProtoSize &size) const { size.add_uint64(1, this->address); }
bool BluetoothGATTReadRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->address = value.as_uint64();
      break;
    case 2:
      this->handle = value.as_uint32();
      break;
    default:
      return false;
  }
  return true;
}
void BluetoothGATTReadResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
  buffer.encode_bytes(3, this->data_ptr_, this->data_len_);
}
void BluetoothGATTReadResponse::calculate_size(ProtoSize &size) const {
  size.add_uint64(1, this->address);
  size.add_uint32(1, this->handle);
  size.add_length(1, this->data_len_);
}
bool BluetoothGATTWriteRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->address = value.as_uint64();
      break;
    case 2:
      this->handle = value.as_uint32();
      break;
    case 3:
      this->response = value.as_bool();
      break;
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTWriteRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 4: {
      // Use raw data directly to avoid allocation
      this->data = value.data();
      this->data_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTReadDescriptorRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->address = value.as_uint64();
      break;
    case 2:
      this->handle = value.as_uint32();
      break;
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTWriteDescriptorRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->address = value.as_uint64();
      break;
    case 2:
      this->handle = value.as_uint32();
      break;
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTWriteDescriptorRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3: {
      // Use raw data directly to avoid allocation
      this->data = value.data();
      this->data_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTNotifyRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->address = value.as_uint64();
      break;
    case 2:
      this->handle = value.as_uint32();
      break;
    case 3:
      this->enable = value.as_bool();
      break;
    default:
      return false;
  }
  return true;
}
void BluetoothGATTNotifyDataResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
  buffer.encode_bytes(3, this->data_ptr_, this->data_len_);
}
void BluetoothGATTNotifyDataResponse::calculate_size(ProtoSize &size) const {
  size.add_uint64(1, this->address);
  size.add_uint32(1, this->handle);
  size.add_length(1, this->data_len_);
}
void BluetoothConnectionsFreeResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->free);
  buffer.encode_uint32(2, this->limit);
  for (const auto &it : this->allocated) {
    if (it != 0) {
      buffer.encode_uint64(3, it, true);
    }
  }
}
void BluetoothConnectionsFreeResponse::calculate_size(ProtoSize &size) const {
  size.add_uint32(1, this->free);
  size.add_uint32(1, this->limit);
  for (const auto &it : this->allocated) {
    if (it != 0) {
      size.add_uint64_force(1, it);
    }
  }
}
void BluetoothGATTErrorResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
  buffer.encode_int32(3, this->error);
}
void BluetoothGATTErrorResponse::calculate_size(ProtoSize &size) const {
  size.add_uint64(1, this->address);
  size.add_uint32(1, this->handle);
  size.add_int32(1, this->error);
}
void BluetoothGATTWriteResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
}
void BluetoothGATTWriteResponse::calculate_size(ProtoSize &size) const {
  size.add_uint64(1, this->address);
  size.add_uint32(1, this->handle);
}
void BluetoothGATTNotifyResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
}
void BluetoothGATTNotifyResponse::calculate_size(ProtoSize &size) const {
  size.add_uint64(1, this->address);
  size.add_uint32(1, this->handle);
}
void BluetoothDevicePairingResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_bool(2, this->paired);
  buffer.encode_int32(3, this->error);
}
void BluetoothDevicePairingResponse::calculate_size(ProtoSize &size) const {
  size.add_uint64(1, this->address);
  size.add_bool(1, this->paired);
  size.add_int32(1, this->error);
}
void BluetoothDeviceUnpairingResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_bool(2, this->success);
  buffer.encode_int32(3, this->error);
}
void BluetoothDeviceUnpairingResponse::calculate_size(ProtoSize &size) const {
  size.add_uint64(1, this->address);
  size.add_bool(1, this->success);
  size.add_int32(1, this->error);
}
void BluetoothDeviceClearCacheResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_bool(2, this->success);
  buffer.encode_int32(3, this->error);
}
void BluetoothDeviceClearCacheResponse::calculate_size(ProtoSize &size) const {
  size.add_uint64(1, this->address);
  size.add_bool(1, this->success);
  size.add_int32(1, this->error);
}
void BluetoothScannerStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, static_cast<uint32_t>(this->state));
  buffer.encode_uint32(2, static_cast<uint32_t>(this->mode));
  buffer.encode_uint32(3, static_cast<uint32_t>(this->configured_mode));
}
void BluetoothScannerStateResponse::calculate_size(ProtoSize &size) const {
  size.add_uint32(1, static_cast<uint32_t>(this->state));
  size.add_uint32(1, static_cast<uint32_t>(this->mode));
  size.add_uint32(1, static_cast<uint32_t>(this->configured_mode));
}
bool BluetoothScannerSetModeRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->mode = static_cast<enums::BluetoothScannerMode>(value.as_uint32());
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_VOICE_ASSISTANT
bool SubscribeVoiceAssistantRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->subscribe = value.as_bool();
      break;
    case 2:
      this->flags = value.as_uint32();
      break;
    default:
      return false;
  }
  return true;
}
void VoiceAssistantAudioSettings::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->noise_suppression_level);
  buffer.encode_uint32(2, this->auto_gain);
  buffer.encode_float(3, this->volume_multiplier);
}
void VoiceAssistantAudioSettings::calculate_size(ProtoSize &size) const {
  size.add_uint32(1, this->noise_suppression_level);
  size.add_uint32(1, this->auto_gain);
  size.add_float(1, this->volume_multiplier);
}
void VoiceAssistantRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_bool(1, this->start);
  buffer.encode_string(2, this->conversation_id_ref_);
  buffer.encode_uint32(3, this->flags);
  buffer.encode_message(4, this->audio_settings);
  buffer.encode_string(5, this->wake_word_phrase_ref_);
}
void VoiceAssistantRequest::calculate_size(ProtoSize &size) const {
  size.add_bool(1, this->start);
  size.add_length(1, this->conversation_id_ref_.size());
  size.add_uint32(1, this->flags);
  size.add_message_object(1, this->audio_settings);
  size.add_length(1, this->wake_word_phrase_ref_.size());
}
bool VoiceAssistantResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->port = value.as_uint32();
      break;
    case 2:
      this->error = value.as_bool();
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantEventData::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1:
      this->name = value.as_string();
      break;
    case 2:
      this->value = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantEventResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->event_type = static_cast<enums::VoiceAssistantEvent>(value.as_uint32());
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantEventResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2:
      this->data.emplace_back();
      value.decode_to_message(this->data.back());
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantAudio::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->end = value.as_bool();
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantAudio::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1:
      this->data = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
void VoiceAssistantAudio::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_bytes(1, this->data_ptr_, this->data_len_);
  buffer.encode_bool(2, this->end);
}
void VoiceAssistantAudio::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->data_len_);
  size.add_bool(1, this->end);
}
bool VoiceAssistantTimerEventResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->event_type = static_cast<enums::VoiceAssistantTimerEvent>(value.as_uint32());
      break;
    case 4:
      this->total_seconds = value.as_uint32();
      break;
    case 5:
      this->seconds_left = value.as_uint32();
      break;
    case 6:
      this->is_active = value.as_bool();
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantTimerEventResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2:
      this->timer_id = value.as_string();
      break;
    case 3:
      this->name = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantAnnounceRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 4:
      this->start_conversation = value.as_bool();
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantAnnounceRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1:
      this->media_id = value.as_string();
      break;
    case 2:
      this->text = value.as_string();
      break;
    case 3:
      this->preannounce_media_id = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
void VoiceAssistantAnnounceFinished::encode(ProtoWriteBuffer buffer) const { buffer.encode_bool(1, this->success); }
void VoiceAssistantAnnounceFinished::calculate_size(ProtoSize &size) const { size.add_bool(1, this->success); }
void VoiceAssistantWakeWord::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->id_ref_);
  buffer.encode_string(2, this->wake_word_ref_);
  for (auto &it : this->trained_languages) {
    buffer.encode_string(3, it, true);
  }
}
void VoiceAssistantWakeWord::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->id_ref_.size());
  size.add_length(1, this->wake_word_ref_.size());
  if (!this->trained_languages.empty()) {
    for (const auto &it : this->trained_languages) {
      size.add_length_force(1, it.size());
    }
  }
}
bool VoiceAssistantExternalWakeWord::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 5:
      this->model_size = value.as_uint32();
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantExternalWakeWord::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1:
      this->id = value.as_string();
      break;
    case 2:
      this->wake_word = value.as_string();
      break;
    case 3:
      this->trained_languages.push_back(value.as_string());
      break;
    case 4:
      this->model_type = value.as_string();
      break;
    case 6:
      this->model_hash = value.as_string();
      break;
    case 7:
      this->url = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantConfigurationRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1:
      this->external_wake_words.emplace_back();
      value.decode_to_message(this->external_wake_words.back());
      break;
    default:
      return false;
  }
  return true;
}
void VoiceAssistantConfigurationResponse::encode(ProtoWriteBuffer buffer) const {
  for (auto &it : this->available_wake_words) {
    buffer.encode_message(1, it, true);
  }
  for (const auto &it : *this->active_wake_words) {
    buffer.encode_string(2, it, true);
  }
  buffer.encode_uint32(3, this->max_active_wake_words);
}
void VoiceAssistantConfigurationResponse::calculate_size(ProtoSize &size) const {
  size.add_repeated_message(1, this->available_wake_words);
  if (!this->active_wake_words->empty()) {
    for (const auto &it : *this->active_wake_words) {
      size.add_length_force(1, it.size());
    }
  }
  size.add_uint32(1, this->max_active_wake_words);
}
bool VoiceAssistantSetConfiguration::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1:
      this->active_wake_words.push_back(value.as_string());
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
void ListEntitiesAlarmControlPanelResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
  buffer.encode_uint32(8, this->supported_features);
  buffer.encode_bool(9, this->requires_code);
  buffer.encode_bool(10, this->requires_code_to_arm);
#ifdef USE_DEVICES
  buffer.encode_uint32(11, this->device_id);
#endif
}
void ListEntitiesAlarmControlPanelResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_uint32(1, this->supported_features);
  size.add_bool(1, this->requires_code);
  size.add_bool(1, this->requires_code_to_arm);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void AlarmControlPanelStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_uint32(2, static_cast<uint32_t>(this->state));
#ifdef USE_DEVICES
  buffer.encode_uint32(3, this->device_id);
#endif
}
void AlarmControlPanelStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_uint32(1, static_cast<uint32_t>(this->state));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool AlarmControlPanelCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->command = static_cast<enums::AlarmControlPanelStateCommand>(value.as_uint32());
      break;
#ifdef USE_DEVICES
    case 4:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool AlarmControlPanelCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3:
      this->code = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
bool AlarmControlPanelCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_TEXT
void ListEntitiesTextResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
  buffer.encode_uint32(8, this->min_length);
  buffer.encode_uint32(9, this->max_length);
  buffer.encode_string(10, this->pattern_ref_);
  buffer.encode_uint32(11, static_cast<uint32_t>(this->mode));
#ifdef USE_DEVICES
  buffer.encode_uint32(12, this->device_id);
#endif
}
void ListEntitiesTextResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_uint32(1, this->min_length);
  size.add_uint32(1, this->max_length);
  size.add_length(1, this->pattern_ref_.size());
  size.add_uint32(1, static_cast<uint32_t>(this->mode));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void TextStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_string(2, this->state_ref_);
  buffer.encode_bool(3, this->missing_state);
#ifdef USE_DEVICES
  buffer.encode_uint32(4, this->device_id);
#endif
}
void TextStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_length(1, this->state_ref_.size());
  size.add_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool TextCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
#ifdef USE_DEVICES
    case 3:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool TextCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2:
      this->state = value.as_string();
      break;
    default:
      return false;
  }
  return true;
}
bool TextCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_DATETIME_DATE
void ListEntitiesDateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  buffer.encode_uint32(8, this->device_id);
#endif
}
void ListEntitiesDateResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void DateStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->missing_state);
  buffer.encode_uint32(3, this->year);
  buffer.encode_uint32(4, this->month);
  buffer.encode_uint32(5, this->day);
#ifdef USE_DEVICES
  buffer.encode_uint32(6, this->device_id);
#endif
}
void DateStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_bool(1, this->missing_state);
  size.add_uint32(1, this->year);
  size.add_uint32(1, this->month);
  size.add_uint32(1, this->day);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool DateCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->year = value.as_uint32();
      break;
    case 3:
      this->month = value.as_uint32();
      break;
    case 4:
      this->day = value.as_uint32();
      break;
#ifdef USE_DEVICES
    case 5:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool DateCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_DATETIME_TIME
void ListEntitiesTimeResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  buffer.encode_uint32(8, this->device_id);
#endif
}
void ListEntitiesTimeResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void TimeStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->missing_state);
  buffer.encode_uint32(3, this->hour);
  buffer.encode_uint32(4, this->minute);
  buffer.encode_uint32(5, this->second);
#ifdef USE_DEVICES
  buffer.encode_uint32(6, this->device_id);
#endif
}
void TimeStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_bool(1, this->missing_state);
  size.add_uint32(1, this->hour);
  size.add_uint32(1, this->minute);
  size.add_uint32(1, this->second);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool TimeCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->hour = value.as_uint32();
      break;
    case 3:
      this->minute = value.as_uint32();
      break;
    case 4:
      this->second = value.as_uint32();
      break;
#ifdef USE_DEVICES
    case 5:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool TimeCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_EVENT
void ListEntitiesEventResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
  buffer.encode_string(8, this->device_class_ref_);
  for (const char *it : *this->event_types) {
    buffer.encode_string(9, it, strlen(it), true);
  }
#ifdef USE_DEVICES
  buffer.encode_uint32(10, this->device_id);
#endif
}
void ListEntitiesEventResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_length(1, this->device_class_ref_.size());
  if (!this->event_types->empty()) {
    for (const char *it : *this->event_types) {
      size.add_length_force(1, strlen(it));
    }
  }
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void EventResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_string(2, this->event_type_ref_);
#ifdef USE_DEVICES
  buffer.encode_uint32(3, this->device_id);
#endif
}
void EventResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_length(1, this->event_type_ref_.size());
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
#endif
#ifdef USE_VALVE
void ListEntitiesValveResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
  buffer.encode_string(8, this->device_class_ref_);
  buffer.encode_bool(9, this->assumed_state);
  buffer.encode_bool(10, this->supports_position);
  buffer.encode_bool(11, this->supports_stop);
#ifdef USE_DEVICES
  buffer.encode_uint32(12, this->device_id);
#endif
}
void ListEntitiesValveResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_length(1, this->device_class_ref_.size());
  size.add_bool(1, this->assumed_state);
  size.add_bool(1, this->supports_position);
  size.add_bool(1, this->supports_stop);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void ValveStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_float(2, this->position);
  buffer.encode_uint32(3, static_cast<uint32_t>(this->current_operation));
#ifdef USE_DEVICES
  buffer.encode_uint32(4, this->device_id);
#endif
}
void ValveStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_float(1, this->position);
  size.add_uint32(1, static_cast<uint32_t>(this->current_operation));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool ValveCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->has_position = value.as_bool();
      break;
    case 4:
      this->stop = value.as_bool();
      break;
#ifdef USE_DEVICES
    case 5:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool ValveCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    case 3:
      this->position = value.as_float();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_DATETIME_DATETIME
void ListEntitiesDateTimeResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  buffer.encode_uint32(8, this->device_id);
#endif
}
void ListEntitiesDateTimeResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void DateTimeStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->missing_state);
  buffer.encode_fixed32(3, this->epoch_seconds);
#ifdef USE_DEVICES
  buffer.encode_uint32(4, this->device_id);
#endif
}
void DateTimeStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_bool(1, this->missing_state);
  size.add_fixed32(1, this->epoch_seconds);
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool DateTimeCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
#ifdef USE_DEVICES
    case 3:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool DateTimeCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    case 2:
      this->epoch_seconds = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_UPDATE
void ListEntitiesUpdateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id_ref_);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name_ref_);
#ifdef USE_ENTITY_ICON
  buffer.encode_string(5, this->icon_ref_);
#endif
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_uint32(7, static_cast<uint32_t>(this->entity_category));
  buffer.encode_string(8, this->device_class_ref_);
#ifdef USE_DEVICES
  buffer.encode_uint32(9, this->device_id);
#endif
}
void ListEntitiesUpdateResponse::calculate_size(ProtoSize &size) const {
  size.add_length(1, this->object_id_ref_.size());
  size.add_fixed32(1, this->key);
  size.add_length(1, this->name_ref_.size());
#ifdef USE_ENTITY_ICON
  size.add_length(1, this->icon_ref_.size());
#endif
  size.add_bool(1, this->disabled_by_default);
  size.add_uint32(1, static_cast<uint32_t>(this->entity_category));
  size.add_length(1, this->device_class_ref_.size());
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
void UpdateStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->missing_state);
  buffer.encode_bool(3, this->in_progress);
  buffer.encode_bool(4, this->has_progress);
  buffer.encode_float(5, this->progress);
  buffer.encode_string(6, this->current_version_ref_);
  buffer.encode_string(7, this->latest_version_ref_);
  buffer.encode_string(8, this->title_ref_);
  buffer.encode_string(9, this->release_summary_ref_);
  buffer.encode_string(10, this->release_url_ref_);
#ifdef USE_DEVICES
  buffer.encode_uint32(11, this->device_id);
#endif
}
void UpdateStateResponse::calculate_size(ProtoSize &size) const {
  size.add_fixed32(1, this->key);
  size.add_bool(1, this->missing_state);
  size.add_bool(1, this->in_progress);
  size.add_bool(1, this->has_progress);
  size.add_float(1, this->progress);
  size.add_length(1, this->current_version_ref_.size());
  size.add_length(1, this->latest_version_ref_.size());
  size.add_length(1, this->title_ref_.size());
  size.add_length(1, this->release_summary_ref_.size());
  size.add_length(1, this->release_url_ref_.size());
#ifdef USE_DEVICES
  size.add_uint32(1, this->device_id);
#endif
}
bool UpdateCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2:
      this->command = static_cast<enums::UpdateCommand>(value.as_uint32());
      break;
#ifdef USE_DEVICES
    case 3:
      this->device_id = value.as_uint32();
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool UpdateCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_ZWAVE_PROXY
bool ZWaveProxyFrame::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      // Use raw data directly to avoid allocation
      this->data = value.data();
      this->data_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
void ZWaveProxyFrame::encode(ProtoWriteBuffer buffer) const { buffer.encode_bytes(1, this->data, this->data_len); }
void ZWaveProxyFrame::calculate_size(ProtoSize &size) const { size.add_length(1, this->data_len); }
bool ZWaveProxyRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1:
      this->type = static_cast<enums::ZWaveProxyRequestType>(value.as_uint32());
      break;
    default:
      return false;
  }
  return true;
}
bool ZWaveProxyRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      // Use raw data directly to avoid allocation
      this->data = value.data();
      this->data_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
void ZWaveProxyRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, static_cast<uint32_t>(this->type));
  buffer.encode_bytes(2, this->data, this->data_len);
}
void ZWaveProxyRequest::calculate_size(ProtoSize &size) const {
  size.add_uint32(1, static_cast<uint32_t>(this->type));
  size.add_length(2, this->data_len);
}
#endif

}  // namespace esphome::api
