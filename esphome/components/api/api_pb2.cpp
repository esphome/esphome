// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#include "api_pb2.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>

namespace esphome::api {

bool HelloRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->api_version_major = value;
      break;
    case 3:
      this->api_version_minor = value;
      break;
    default:
      return false;
  }
  return true;
}
bool HelloRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->client_info = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    default:
      return false;
  }
  return true;
}
void HelloResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->api_version_major);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->api_version_minor);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->server_info);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, this->name);
}
uint32_t HelloResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->api_version_major);
  size += ProtoSize::calc_uint32(1, this->api_version_minor);
  size += ProtoSize::calc_length(1, this->server_info.size());
  size += ProtoSize::calc_length(1, this->name.size());
  return size;
}
#ifdef USE_AREAS
void AreaInfo::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->area_id);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->name);
}
uint32_t AreaInfo::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->area_id);
  size += ProtoSize::calc_length(1, this->name.size());
  return size;
}
#endif
#ifdef USE_DEVICES
void DeviceInfo::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->device_id);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->name);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->area_id);
}
uint32_t DeviceInfo::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->device_id);
  size += ProtoSize::calc_length(1, this->name.size());
  size += ProtoSize::calc_uint32(1, this->area_id);
  return size;
}
#endif
#ifdef USE_SERIAL_PROXY
void SerialProxyInfo::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->name);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->port_type));
}
uint32_t SerialProxyInfo::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->name.size());
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->port_type));
  return size;
}
#endif
void DeviceInfoResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->name);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->mac_address);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, this->esphome_version);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->compilation_time);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, this->model);
#ifdef USE_DEEP_SLEEP
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->has_deep_sleep);
#endif
#ifdef ESPHOME_PROJECT_NAME
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->project_name);
#endif
#ifdef ESPHOME_PROJECT_NAME
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, this->project_version);
#endif
#ifdef USE_WEBSERVER
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, this->webserver_port);
#endif
#ifdef USE_BLUETOOTH_PROXY
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 15, this->bluetooth_proxy_feature_flags);
#endif
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 12, this->manufacturer);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 13, this->friendly_name);
#ifdef USE_VOICE_ASSISTANT
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 17, this->voice_assistant_feature_flags);
#endif
#ifdef USE_AREAS
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 16, this->suggested_area);
#endif
#ifdef USE_BLUETOOTH_PROXY
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 18, this->bluetooth_mac_address);
#endif
#ifdef USE_API_NOISE
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 19, this->api_encryption_supported);
#endif
#ifdef USE_DEVICES
  for (const auto &it : this->devices) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 20, it);
  }
#endif
#ifdef USE_AREAS
  for (const auto &it : this->areas) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 21, it);
  }
#endif
#ifdef USE_AREAS
  proto_encode_optional_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 22, this->area);
#endif
#ifdef USE_ZWAVE_PROXY
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 23, this->zwave_proxy_feature_flags);
#endif
#ifdef USE_ZWAVE_PROXY
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 24, this->zwave_home_id);
#endif
#ifdef USE_SERIAL_PROXY
  for (const auto &it : this->serial_proxies) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 25, it);
  }
#endif
}
uint32_t DeviceInfoResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->name.size());
  size += ProtoSize::calc_length(1, this->mac_address.size());
  size += ProtoSize::calc_length(1, this->esphome_version.size());
  size += ProtoSize::calc_length(1, this->compilation_time.size());
  size += ProtoSize::calc_length(1, this->model.size());
#ifdef USE_DEEP_SLEEP
  size += ProtoSize::calc_bool(1, this->has_deep_sleep);
#endif
#ifdef ESPHOME_PROJECT_NAME
  size += ProtoSize::calc_length(1, this->project_name.size());
#endif
#ifdef ESPHOME_PROJECT_NAME
  size += ProtoSize::calc_length(1, this->project_version.size());
#endif
#ifdef USE_WEBSERVER
  size += ProtoSize::calc_uint32(1, this->webserver_port);
#endif
#ifdef USE_BLUETOOTH_PROXY
  size += ProtoSize::calc_uint32(1, this->bluetooth_proxy_feature_flags);
#endif
  size += ProtoSize::calc_length(1, this->manufacturer.size());
  size += ProtoSize::calc_length(1, this->friendly_name.size());
#ifdef USE_VOICE_ASSISTANT
  size += ProtoSize::calc_uint32(2, this->voice_assistant_feature_flags);
#endif
#ifdef USE_AREAS
  size += ProtoSize::calc_length(2, this->suggested_area.size());
#endif
#ifdef USE_BLUETOOTH_PROXY
  size += ProtoSize::calc_length(2, this->bluetooth_mac_address.size());
#endif
#ifdef USE_API_NOISE
  size += ProtoSize::calc_bool(2, this->api_encryption_supported);
#endif
#ifdef USE_DEVICES
  for (const auto &it : this->devices) {
    size += ProtoSize::calc_message_force(2, it.calculate_size());
  }
#endif
#ifdef USE_AREAS
  for (const auto &it : this->areas) {
    size += ProtoSize::calc_message_force(2, it.calculate_size());
  }
#endif
#ifdef USE_AREAS
  size += ProtoSize::calc_message(2, this->area.calculate_size());
#endif
#ifdef USE_ZWAVE_PROXY
  size += ProtoSize::calc_uint32(2, this->zwave_proxy_feature_flags);
#endif
#ifdef USE_ZWAVE_PROXY
  size += ProtoSize::calc_uint32(2, this->zwave_home_id);
#endif
#ifdef USE_SERIAL_PROXY
  for (const auto &it : this->serial_proxies) {
    size += ProtoSize::calc_message_force(2, it.calculate_size());
  }
#endif
  return size;
}
#ifdef USE_BINARY_SENSOR
void ListEntitiesBinarySensorResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->device_class);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->is_status_binary_sensor);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->icon);
#endif
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, this->device_id);
#endif
}
uint32_t ListEntitiesBinarySensorResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
  size += ProtoSize::calc_length(1, this->device_class.size());
  size += ProtoSize::calc_bool(1, this->is_status_binary_sensor);
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void BinarySensorStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
}
uint32_t BinarySensorStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, this->state);
  size += ProtoSize::calc_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
#endif
#ifdef USE_COVER
void ListEntitiesCoverResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->assumed_state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->supports_position);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->supports_tilt);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, this->icon);
#endif
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(this->entity_category));
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 12, this->supports_stop);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->device_id);
#endif
}
uint32_t ListEntitiesCoverResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
  size += ProtoSize::calc_bool(1, this->assumed_state);
  size += ProtoSize::calc_bool(1, this->supports_position);
  size += ProtoSize::calc_bool(1, this->supports_tilt);
  size += ProtoSize::calc_length(1, this->device_class.size());
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_bool(1, this->supports_stop);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void CoverStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->position);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 4, this->tilt);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, static_cast<uint32_t>(this->current_operation));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->device_id);
#endif
}
uint32_t CoverStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, this->position);
  size += ProtoSize::calc_float(1, this->tilt);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->current_operation));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool CoverCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 4:
      this->has_position = value != 0;
      break;
    case 6:
      this->has_tilt = value != 0;
      break;
    case 8:
      this->stop = value != 0;
      break;
#ifdef USE_DEVICES
    case 9:
      this->device_id = value;
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
void ListEntitiesFanResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->supports_oscillation);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->supports_speed);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->supports_direction);
  proto_encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->supported_speed_count);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, this->icon);
#endif
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(this->entity_category));
  for (const char *it : *this->supported_preset_modes) {
    proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 12, it, strlen(it), true);
  }
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->device_id);
#endif
}
uint32_t ListEntitiesFanResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
  size += ProtoSize::calc_bool(1, this->supports_oscillation);
  size += ProtoSize::calc_bool(1, this->supports_speed);
  size += ProtoSize::calc_bool(1, this->supports_direction);
  size += ProtoSize::calc_int32(1, this->supported_speed_count);
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  if (!this->supported_preset_modes->empty()) {
    for (const char *it : *this->supported_preset_modes) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void FanStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->oscillating);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, static_cast<uint32_t>(this->direction));
  proto_encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->speed_level);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 7, this->preset_mode);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_id);
#endif
}
uint32_t FanStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, this->state);
  size += ProtoSize::calc_bool(1, this->oscillating);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->direction));
  size += ProtoSize::calc_int32(1, this->speed_level);
  size += ProtoSize::calc_length(1, this->preset_mode.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool FanCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->has_state = value != 0;
      break;
    case 3:
      this->state = value != 0;
      break;
    case 6:
      this->has_oscillating = value != 0;
      break;
    case 7:
      this->oscillating = value != 0;
      break;
    case 8:
      this->has_direction = value != 0;
      break;
    case 9:
      this->direction = static_cast<enums::FanDirection>(value);
      break;
    case 10:
      this->has_speed_level = value != 0;
      break;
    case 11:
      this->speed_level = static_cast<int32_t>(value);
      break;
    case 12:
      this->has_preset_mode = value != 0;
      break;
#ifdef USE_DEVICES
    case 14:
      this->device_id = value;
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool FanCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 13: {
      this->preset_mode = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
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
void ListEntitiesLightResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
  for (const auto &it : *this->supported_color_modes) {
    proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, static_cast<uint32_t>(it), true);
  }
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 9, this->min_mireds);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 10, this->max_mireds);
  for (const char *it : *this->effects) {
    proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 11, it, strlen(it), true);
  }
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 13, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 14, this->icon);
#endif
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 15, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 16, this->device_id);
#endif
}
uint32_t ListEntitiesLightResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
  if (!this->supported_color_modes->empty()) {
    for (const auto &it : *this->supported_color_modes) {
      size += ProtoSize::calc_uint32_force(1, static_cast<uint32_t>(it));
    }
  }
  size += ProtoSize::calc_float(1, this->min_mireds);
  size += ProtoSize::calc_float(1, this->max_mireds);
  if (!this->effects->empty()) {
    for (const char *it : *this->effects) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(2, this->device_id);
#endif
  return size;
}
void LightStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->brightness);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(this->color_mode));
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 10, this->color_brightness);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 4, this->red);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 5, this->green);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 6, this->blue);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 7, this->white);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 8, this->color_temperature);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 12, this->cold_white);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 13, this->warm_white);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, this->effect);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 14, this->device_id);
#endif
}
uint32_t LightStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, this->state);
  size += ProtoSize::calc_float(1, this->brightness);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->color_mode));
  size += ProtoSize::calc_float(1, this->color_brightness);
  size += ProtoSize::calc_float(1, this->red);
  size += ProtoSize::calc_float(1, this->green);
  size += ProtoSize::calc_float(1, this->blue);
  size += ProtoSize::calc_float(1, this->white);
  size += ProtoSize::calc_float(1, this->color_temperature);
  size += ProtoSize::calc_float(1, this->cold_white);
  size += ProtoSize::calc_float(1, this->warm_white);
  size += ProtoSize::calc_length(1, this->effect.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool LightCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->has_state = value != 0;
      break;
    case 3:
      this->state = value != 0;
      break;
    case 4:
      this->has_brightness = value != 0;
      break;
    case 22:
      this->has_color_mode = value != 0;
      break;
    case 23:
      this->color_mode = static_cast<enums::ColorMode>(value);
      break;
    case 20:
      this->has_color_brightness = value != 0;
      break;
    case 6:
      this->has_rgb = value != 0;
      break;
    case 10:
      this->has_white = value != 0;
      break;
    case 12:
      this->has_color_temperature = value != 0;
      break;
    case 24:
      this->has_cold_white = value != 0;
      break;
    case 26:
      this->has_warm_white = value != 0;
      break;
    case 14:
      this->has_transition_length = value != 0;
      break;
    case 15:
      this->transition_length = value;
      break;
    case 16:
      this->has_flash_length = value != 0;
      break;
    case 17:
      this->flash_length = value;
      break;
    case 18:
      this->has_effect = value != 0;
      break;
#ifdef USE_DEVICES
    case 28:
      this->device_id = value;
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
      this->effect = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
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
void ListEntitiesSensorResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, this->unit_of_measurement);
  proto_encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 7, this->accuracy_decimals);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 8, this->force_update);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_class);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(this->state_class));
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 12, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 14, this->device_id);
#endif
}
uint32_t ListEntitiesSensorResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_length(1, this->unit_of_measurement.size());
  size += ProtoSize::calc_int32(1, this->accuracy_decimals);
  size += ProtoSize::calc_bool(1, this->force_update);
  size += ProtoSize::calc_length(1, this->device_class.size());
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->state_class));
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void SensorStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
}
uint32_t SensorStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, this->state);
  size += ProtoSize::calc_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
#endif
#ifdef USE_SWITCH
void ListEntitiesSwitchResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->assumed_state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, static_cast<uint32_t>(this->entity_category));
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_class);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, this->device_id);
#endif
}
uint32_t ListEntitiesSwitchResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->assumed_state);
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_length(1, this->device_class.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void SwitchStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->device_id);
#endif
}
uint32_t SwitchStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, this->state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool SwitchCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->state = value != 0;
      break;
#ifdef USE_DEVICES
    case 3:
      this->device_id = value;
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
void ListEntitiesTextSensorResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_id);
#endif
}
uint32_t ListEntitiesTextSensorResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_length(1, this->device_class.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void TextSensorStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
}
uint32_t TextSensorStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_length(1, this->state.size());
  size += ProtoSize::calc_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
#endif
bool SubscribeLogsRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->level = static_cast<enums::LogLevel>(value);
      break;
    case 2:
      this->dump_config = value != 0;
      break;
    default:
      return false;
  }
  return true;
}
void SubscribeLogsResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, static_cast<uint32_t>(this->level));
  proto_encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 3, this->message_ptr_, this->message_len_);
}
uint32_t SubscribeLogsResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->level));
  size += ProtoSize::calc_length(1, this->message_len_);
  return size;
}
#ifdef USE_API_NOISE
bool NoiseEncryptionSetKeyRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->key = value.data();
      this->key_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
void NoiseEncryptionSetKeyResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 1, this->success);
}
uint32_t NoiseEncryptionSetKeyResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_bool(1, this->success);
  return size;
}
#endif
#ifdef USE_API_HOMEASSISTANT_SERVICES
void HomeassistantServiceMap::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->value);
}
uint32_t HomeassistantServiceMap::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->key.size());
  size += ProtoSize::calc_length(1, this->value.size());
  return size;
}
void HomeassistantActionRequest::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->service);
  for (auto &it : this->data) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 2, it);
  }
  for (auto &it : this->data_template) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 3, it);
  }
  for (auto &it : this->variables) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 4, it);
  }
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->is_event);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->call_id);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->wants_response);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->response_template);
#endif
}
uint32_t HomeassistantActionRequest::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->service.size());
  if (!this->data.empty()) {
    for (const auto &it : this->data) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  if (!this->data_template.empty()) {
    for (const auto &it : this->data_template) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  if (!this->variables.empty()) {
    for (const auto &it : this->variables) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  size += ProtoSize::calc_bool(1, this->is_event);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  size += ProtoSize::calc_uint32(1, this->call_id);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  size += ProtoSize::calc_bool(1, this->wants_response);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  size += ProtoSize::calc_length(1, this->response_template.size());
#endif
  return size;
}
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
bool HomeassistantActionResponse::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->call_id = value;
      break;
    case 2:
      this->success = value != 0;
      break;
    default:
      return false;
  }
  return true;
}
bool HomeassistantActionResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3: {
      this->error_message = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
    case 4: {
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
void SubscribeHomeAssistantStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->entity_id);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->attribute);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->once);
}
uint32_t SubscribeHomeAssistantStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->entity_id.size());
  size += ProtoSize::calc_length(1, this->attribute.size());
  size += ProtoSize::calc_bool(1, this->once);
  return size;
}
bool HomeAssistantStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->entity_id = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 2: {
      this->state = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 3: {
      this->attribute = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    default:
      return false;
  }
  return true;
}
#endif
bool DSTRule::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->time_seconds = decode_zigzag32(static_cast<uint32_t>(value));
      break;
    case 2:
      this->day = value;
      break;
    case 3:
      this->type = static_cast<enums::DSTRuleType>(value);
      break;
    case 4:
      this->month = value;
      break;
    case 5:
      this->week = value;
      break;
    case 6:
      this->day_of_week = value;
      break;
    default:
      return false;
  }
  return true;
}
bool ParsedTimezone::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->std_offset_seconds = decode_zigzag32(static_cast<uint32_t>(value));
      break;
    case 2:
      this->dst_offset_seconds = decode_zigzag32(static_cast<uint32_t>(value));
      break;
    default:
      return false;
  }
  return true;
}
bool ParsedTimezone::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3:
      value.decode_to_message(this->dst_start);
      break;
    case 4:
      value.decode_to_message(this->dst_end);
      break;
    default:
      return false;
  }
  return true;
}
bool GetTimeResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->timezone = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 3:
      value.decode_to_message(this->parsed_timezone);
      break;
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
void ListEntitiesServicesArgument::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->name);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->type));
}
uint32_t ListEntitiesServicesArgument::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->name.size());
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->type));
  return size;
}
void ListEntitiesServicesResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->name);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  for (auto &it : this->args) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 3, it);
  }
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, static_cast<uint32_t>(this->supports_response));
}
uint32_t ListEntitiesServicesResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->name.size());
  size += 5;
  if (!this->args.empty()) {
    for (const auto &it : this->args) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->supports_response));
  return size;
}
bool ExecuteServiceArgument::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->bool_ = value != 0;
      break;
    case 2:
      this->legacy_int = static_cast<int32_t>(value);
      break;
    case 5:
      this->int_ = decode_zigzag32(static_cast<uint32_t>(value));
      break;
    case 6:
      this->bool_array.push_back(value != 0);
      break;
    case 7:
      this->int_array.push_back(decode_zigzag32(static_cast<uint32_t>(value)));
      break;
    default:
      return false;
  }
  return true;
}
bool ExecuteServiceArgument::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 4: {
      this->string_ = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
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
bool ExecuteServiceRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
    case 3:
      this->call_id = value;
      break;
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
    case 4:
      this->return_response = value != 0;
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
void ExecuteServiceResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->call_id);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->success);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->error_message);
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  proto_encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 4, this->response_data, this->response_data_len);
#endif
}
uint32_t ExecuteServiceResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->call_id);
  size += ProtoSize::calc_bool(1, this->success);
  size += ProtoSize::calc_length(1, this->error_message.size());
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  size += ProtoSize::calc_length(1, this->response_data_len);
#endif
  return size;
}
#endif
#ifdef USE_CAMERA
void ListEntitiesCameraResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, this->icon);
#endif
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_id);
#endif
}
uint32_t ListEntitiesCameraResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void CameraImageResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 2, this->data_ptr_, this->data_len_);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->done);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
}
uint32_t CameraImageResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_length(1, this->data_len_);
  size += ProtoSize::calc_bool(1, this->done);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool CameraImageRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->single = value != 0;
      break;
    case 2:
      this->stream = value != 0;
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_CLIMATE
void ListEntitiesClimateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->supports_current_temperature);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->supports_two_point_target_temperature);
  for (const auto &it : *this->supported_modes) {
    proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(it), true);
  }
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 8, this->visual_min_temperature);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 9, this->visual_max_temperature);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 10, this->visual_target_temperature_step);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 12, this->supports_action);
  for (const auto &it : *this->supported_fan_modes) {
    proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, static_cast<uint32_t>(it), true);
  }
  for (const auto &it : *this->supported_swing_modes) {
    proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 14, static_cast<uint32_t>(it), true);
  }
  for (const char *it : *this->supported_custom_fan_modes) {
    proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 15, it, strlen(it), true);
  }
  for (const auto &it : *this->supported_presets) {
    proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 16, static_cast<uint32_t>(it), true);
  }
  for (const char *it : *this->supported_custom_presets) {
    proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 17, it, strlen(it), true);
  }
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 18, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 19, this->icon);
#endif
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 20, static_cast<uint32_t>(this->entity_category));
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 21, this->visual_current_temperature_step);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 22, this->supports_current_humidity);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 23, this->supports_target_humidity);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 24, this->visual_min_humidity);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 25, this->visual_max_humidity);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 26, this->device_id);
#endif
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 27, this->feature_flags);
}
uint32_t ListEntitiesClimateResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
  size += ProtoSize::calc_bool(1, this->supports_current_temperature);
  size += ProtoSize::calc_bool(1, this->supports_two_point_target_temperature);
  if (!this->supported_modes->empty()) {
    for (const auto &it : *this->supported_modes) {
      size += ProtoSize::calc_uint32_force(1, static_cast<uint32_t>(it));
    }
  }
  size += ProtoSize::calc_float(1, this->visual_min_temperature);
  size += ProtoSize::calc_float(1, this->visual_max_temperature);
  size += ProtoSize::calc_float(1, this->visual_target_temperature_step);
  size += ProtoSize::calc_bool(1, this->supports_action);
  if (!this->supported_fan_modes->empty()) {
    for (const auto &it : *this->supported_fan_modes) {
      size += ProtoSize::calc_uint32_force(1, static_cast<uint32_t>(it));
    }
  }
  if (!this->supported_swing_modes->empty()) {
    for (const auto &it : *this->supported_swing_modes) {
      size += ProtoSize::calc_uint32_force(1, static_cast<uint32_t>(it));
    }
  }
  if (!this->supported_custom_fan_modes->empty()) {
    for (const char *it : *this->supported_custom_fan_modes) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
  if (!this->supported_presets->empty()) {
    for (const auto &it : *this->supported_presets) {
      size += ProtoSize::calc_uint32_force(2, static_cast<uint32_t>(it));
    }
  }
  if (!this->supported_custom_presets->empty()) {
    for (const char *it : *this->supported_custom_presets) {
      size += ProtoSize::calc_length_force(2, strlen(it));
    }
  }
  size += ProtoSize::calc_bool(2, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(2, this->icon.size());
#endif
  size += ProtoSize::calc_uint32(2, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_float(2, this->visual_current_temperature_step);
  size += ProtoSize::calc_bool(2, this->supports_current_humidity);
  size += ProtoSize::calc_bool(2, this->supports_target_humidity);
  size += ProtoSize::calc_float(2, this->visual_min_humidity);
  size += ProtoSize::calc_float(2, this->visual_max_humidity);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(2, this->device_id);
#endif
  size += ProtoSize::calc_uint32(2, this->feature_flags);
  return size;
}
void ClimateStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->mode));
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->current_temperature);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 4, this->target_temperature);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 5, this->target_temperature_low);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 6, this->target_temperature_high);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, static_cast<uint32_t>(this->action));
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, static_cast<uint32_t>(this->fan_mode));
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(this->swing_mode));
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 11, this->custom_fan_mode);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, static_cast<uint32_t>(this->preset));
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 13, this->custom_preset);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 14, this->current_humidity);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 15, this->target_humidity);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 16, this->device_id);
#endif
}
uint32_t ClimateStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->mode));
  size += ProtoSize::calc_float(1, this->current_temperature);
  size += ProtoSize::calc_float(1, this->target_temperature);
  size += ProtoSize::calc_float(1, this->target_temperature_low);
  size += ProtoSize::calc_float(1, this->target_temperature_high);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->action));
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->fan_mode));
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->swing_mode));
  size += ProtoSize::calc_length(1, this->custom_fan_mode.size());
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->preset));
  size += ProtoSize::calc_length(1, this->custom_preset.size());
  size += ProtoSize::calc_float(1, this->current_humidity);
  size += ProtoSize::calc_float(1, this->target_humidity);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(2, this->device_id);
#endif
  return size;
}
bool ClimateCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->has_mode = value != 0;
      break;
    case 3:
      this->mode = static_cast<enums::ClimateMode>(value);
      break;
    case 4:
      this->has_target_temperature = value != 0;
      break;
    case 6:
      this->has_target_temperature_low = value != 0;
      break;
    case 8:
      this->has_target_temperature_high = value != 0;
      break;
    case 12:
      this->has_fan_mode = value != 0;
      break;
    case 13:
      this->fan_mode = static_cast<enums::ClimateFanMode>(value);
      break;
    case 14:
      this->has_swing_mode = value != 0;
      break;
    case 15:
      this->swing_mode = static_cast<enums::ClimateSwingMode>(value);
      break;
    case 16:
      this->has_custom_fan_mode = value != 0;
      break;
    case 18:
      this->has_preset = value != 0;
      break;
    case 19:
      this->preset = static_cast<enums::ClimatePreset>(value);
      break;
    case 20:
      this->has_custom_preset = value != 0;
      break;
    case 22:
      this->has_target_humidity = value != 0;
      break;
#ifdef USE_DEVICES
    case 24:
      this->device_id = value;
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool ClimateCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 17: {
      this->custom_fan_mode = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 21: {
      this->custom_preset = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
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
#ifdef USE_WATER_HEATER
void ListEntitiesWaterHeaterResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, this->device_id);
#endif
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 8, this->min_temperature);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 9, this->max_temperature);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 10, this->target_temperature_step);
  for (const auto &it : *this->supported_modes) {
    proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(it), true);
  }
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, this->supported_features);
}
uint32_t ListEntitiesWaterHeaterResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  size += ProtoSize::calc_float(1, this->min_temperature);
  size += ProtoSize::calc_float(1, this->max_temperature);
  size += ProtoSize::calc_float(1, this->target_temperature_step);
  if (!this->supported_modes->empty()) {
    for (const auto &it : *this->supported_modes) {
      size += ProtoSize::calc_uint32_force(1, static_cast<uint32_t>(it));
    }
  }
  size += ProtoSize::calc_uint32(1, this->supported_features);
  return size;
}
void WaterHeaterStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 2, this->current_temperature);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->target_temperature);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, static_cast<uint32_t>(this->mode));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->device_id);
#endif
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->state);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 7, this->target_temperature_low);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 8, this->target_temperature_high);
}
uint32_t WaterHeaterStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, this->current_temperature);
  size += ProtoSize::calc_float(1, this->target_temperature);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->mode));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  size += ProtoSize::calc_uint32(1, this->state);
  size += ProtoSize::calc_float(1, this->target_temperature_low);
  size += ProtoSize::calc_float(1, this->target_temperature_high);
  return size;
}
bool WaterHeaterCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->has_fields = value;
      break;
    case 3:
      this->mode = static_cast<enums::WaterHeaterMode>(value);
      break;
#ifdef USE_DEVICES
    case 5:
      this->device_id = value;
      break;
#endif
    case 6:
      this->state = value;
      break;
    default:
      return false;
  }
  return true;
}
bool WaterHeaterCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1:
      this->key = value.as_fixed32();
      break;
    case 4:
      this->target_temperature = value.as_float();
      break;
    case 7:
      this->target_temperature_low = value.as_float();
      break;
    case 8:
      this->target_temperature_high = value.as_float();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_NUMBER
void ListEntitiesNumberResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 6, this->min_value);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 7, this->max_value);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 8, this->step);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(this->entity_category));
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 11, this->unit_of_measurement);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, static_cast<uint32_t>(this->mode));
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 13, this->device_class);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 14, this->device_id);
#endif
}
uint32_t ListEntitiesNumberResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_float(1, this->min_value);
  size += ProtoSize::calc_float(1, this->max_value);
  size += ProtoSize::calc_float(1, this->step);
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_length(1, this->unit_of_measurement.size());
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->mode));
  size += ProtoSize::calc_length(1, this->device_class.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void NumberStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
}
uint32_t NumberStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, this->state);
  size += ProtoSize::calc_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool NumberCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
#ifdef USE_DEVICES
    case 3:
      this->device_id = value;
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
void ListEntitiesSelectResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  for (const char *it : *this->options) {
    proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, it, strlen(it), true);
  }
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_id);
#endif
}
uint32_t ListEntitiesSelectResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  if (!this->options->empty()) {
    for (const char *it : *this->options) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void SelectStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
}
uint32_t SelectStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_length(1, this->state.size());
  size += ProtoSize::calc_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool SelectCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
#ifdef USE_DEVICES
    case 3:
      this->device_id = value;
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
      this->state = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
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
void ListEntitiesSirenResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  for (const char *it : *this->tones) {
    proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 7, it, strlen(it), true);
  }
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 8, this->supports_duration);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->supports_volume);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, this->device_id);
#endif
}
uint32_t ListEntitiesSirenResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  if (!this->tones->empty()) {
    for (const char *it : *this->tones) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
  size += ProtoSize::calc_bool(1, this->supports_duration);
  size += ProtoSize::calc_bool(1, this->supports_volume);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void SirenStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->device_id);
#endif
}
uint32_t SirenStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, this->state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool SirenCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->has_state = value != 0;
      break;
    case 3:
      this->state = value != 0;
      break;
    case 4:
      this->has_tone = value != 0;
      break;
    case 6:
      this->has_duration = value != 0;
      break;
    case 7:
      this->duration = value;
      break;
    case 8:
      this->has_volume = value != 0;
      break;
#ifdef USE_DEVICES
    case 10:
      this->device_id = value;
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool SirenCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 5: {
      this->tone = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
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
void ListEntitiesLockResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 8, this->assumed_state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->supports_open);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 10, this->requires_code);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 11, this->code_format);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, this->device_id);
#endif
}
uint32_t ListEntitiesLockResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_bool(1, this->assumed_state);
  size += ProtoSize::calc_bool(1, this->supports_open);
  size += ProtoSize::calc_bool(1, this->requires_code);
  size += ProtoSize::calc_length(1, this->code_format.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void LockStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->state));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->device_id);
#endif
}
uint32_t LockStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->state));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool LockCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->command = static_cast<enums::LockCommand>(value);
      break;
    case 3:
      this->has_code = value != 0;
      break;
#ifdef USE_DEVICES
    case 5:
      this->device_id = value;
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool LockCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 4: {
      this->code = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
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
void ListEntitiesButtonResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_id);
#endif
}
uint32_t ListEntitiesButtonResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_length(1, this->device_class.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool ButtonCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
#ifdef USE_DEVICES
    case 2:
      this->device_id = value;
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
void MediaPlayerSupportedFormat::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->format);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->sample_rate);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->num_channels);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, static_cast<uint32_t>(this->purpose));
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->sample_bytes);
}
uint32_t MediaPlayerSupportedFormat::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->format.size());
  size += ProtoSize::calc_uint32(1, this->sample_rate);
  size += ProtoSize::calc_uint32(1, this->num_channels);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->purpose));
  size += ProtoSize::calc_uint32(1, this->sample_bytes);
  return size;
}
void ListEntitiesMediaPlayerResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 8, this->supports_pause);
  for (auto &it : this->supported_formats) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 9, it);
  }
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, this->device_id);
#endif
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, this->feature_flags);
}
uint32_t ListEntitiesMediaPlayerResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_bool(1, this->supports_pause);
  if (!this->supported_formats.empty()) {
    for (const auto &it : this->supported_formats) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  size += ProtoSize::calc_uint32(1, this->feature_flags);
  return size;
}
void MediaPlayerStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->state));
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->volume);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 4, this->muted);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->device_id);
#endif
}
uint32_t MediaPlayerStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->state));
  size += ProtoSize::calc_float(1, this->volume);
  size += ProtoSize::calc_bool(1, this->muted);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool MediaPlayerCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->has_command = value != 0;
      break;
    case 3:
      this->command = static_cast<enums::MediaPlayerCommand>(value);
      break;
    case 4:
      this->has_volume = value != 0;
      break;
    case 6:
      this->has_media_url = value != 0;
      break;
    case 8:
      this->has_announcement = value != 0;
      break;
    case 9:
      this->announcement = value != 0;
      break;
#ifdef USE_DEVICES
    case 10:
      this->device_id = value;
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool MediaPlayerCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 7: {
      this->media_url = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
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
bool SubscribeBluetoothLEAdvertisementsRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->flags = value;
      break;
    default:
      return false;
  }
  return true;
}
void BluetoothLERawAdvertisement::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 8);
  proto_encode_varint_raw_64(pos PROTO_ENCODE_DEBUG_ARG, this->address);
  proto_write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 16);
  proto_encode_varint_raw_short(pos PROTO_ENCODE_DEBUG_ARG, encode_zigzag32(this->rssi));
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->address_type);
  proto_write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 34);
  proto_write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, static_cast<uint8_t>(this->data_len));
  proto_encode_raw(pos PROTO_ENCODE_DEBUG_ARG, this->data, this->data_len);
}
uint32_t BluetoothLERawAdvertisement::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64_force(1, this->address);
  size += ProtoSize::calc_sint32_force(1, this->rssi);
  size += this->address_type ? 2 : 0;
  size += 2 + this->data_len;
  return size;
}
void BluetoothLERawAdvertisementsResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  for (uint16_t i = 0; i < this->advertisements_len; i++) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 1, this->advertisements[i]);
  }
}
uint32_t BluetoothLERawAdvertisementsResponse::calculate_size() const {
  uint32_t size = 0;
  for (uint16_t i = 0; i < this->advertisements_len; i++) {
    size += ProtoSize::calc_message_force(1, this->advertisements[i].calculate_size());
  }
  return size;
}
bool BluetoothDeviceRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->address = value;
      break;
    case 2:
      this->request_type = static_cast<enums::BluetoothDeviceRequestType>(value);
      break;
    case 3:
      this->has_address_type = value != 0;
      break;
    case 4:
      this->address_type = value;
      break;
    default:
      return false;
  }
  return true;
}
void BluetoothDeviceConnectionResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->connected);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->mtu);
  proto_encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->error);
}
uint32_t BluetoothDeviceConnectionResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_bool(1, this->connected);
  size += ProtoSize::calc_uint32(1, this->mtu);
  size += ProtoSize::calc_int32(1, this->error);
  return size;
}
bool BluetoothGATTGetServicesRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->address = value;
      break;
    default:
      return false;
  }
  return true;
}
void BluetoothGATTDescriptor::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[0], true);
    proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[1], true);
  }
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->short_uuid);
}
uint32_t BluetoothGATTDescriptor::calculate_size() const {
  uint32_t size = 0;
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    size += ProtoSize::calc_uint64_force(1, this->uuid[0]);
    size += ProtoSize::calc_uint64_force(1, this->uuid[1]);
  }
  size += ProtoSize::calc_uint32(1, this->handle);
  size += ProtoSize::calc_uint32(1, this->short_uuid);
  return size;
}
void BluetoothGATTCharacteristic::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[0], true);
    proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[1], true);
  }
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->properties);
  for (auto &it : this->descriptors) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 4, it);
  }
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->short_uuid);
}
uint32_t BluetoothGATTCharacteristic::calculate_size() const {
  uint32_t size = 0;
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    size += ProtoSize::calc_uint64_force(1, this->uuid[0]);
    size += ProtoSize::calc_uint64_force(1, this->uuid[1]);
  }
  size += ProtoSize::calc_uint32(1, this->handle);
  size += ProtoSize::calc_uint32(1, this->properties);
  if (!this->descriptors.empty()) {
    for (const auto &it : this->descriptors) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  size += ProtoSize::calc_uint32(1, this->short_uuid);
  return size;
}
void BluetoothGATTService::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[0], true);
    proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[1], true);
  }
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  for (auto &it : this->characteristics) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 3, it);
  }
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->short_uuid);
}
uint32_t BluetoothGATTService::calculate_size() const {
  uint32_t size = 0;
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    size += ProtoSize::calc_uint64_force(1, this->uuid[0]);
    size += ProtoSize::calc_uint64_force(1, this->uuid[1]);
  }
  size += ProtoSize::calc_uint32(1, this->handle);
  if (!this->characteristics.empty()) {
    for (const auto &it : this->characteristics) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  size += ProtoSize::calc_uint32(1, this->short_uuid);
  return size;
}
void BluetoothGATTGetServicesResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  for (auto &it : this->services) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 2, it);
  }
}
uint32_t BluetoothGATTGetServicesResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  if (!this->services.empty()) {
    for (const auto &it : this->services) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  return size;
}
void BluetoothGATTGetServicesDoneResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
}
uint32_t BluetoothGATTGetServicesDoneResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  return size;
}
bool BluetoothGATTReadRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->address = value;
      break;
    case 2:
      this->handle = value;
      break;
    default:
      return false;
  }
  return true;
}
void BluetoothGATTReadResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  proto_encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 3, this->data_ptr_, this->data_len_);
}
uint32_t BluetoothGATTReadResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_uint32(1, this->handle);
  size += ProtoSize::calc_length(1, this->data_len_);
  return size;
}
bool BluetoothGATTWriteRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->address = value;
      break;
    case 2:
      this->handle = value;
      break;
    case 3:
      this->response = value != 0;
      break;
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTWriteRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 4: {
      this->data = value.data();
      this->data_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTReadDescriptorRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->address = value;
      break;
    case 2:
      this->handle = value;
      break;
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTWriteDescriptorRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->address = value;
      break;
    case 2:
      this->handle = value;
      break;
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTWriteDescriptorRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3: {
      this->data = value.data();
      this->data_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTNotifyRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->address = value;
      break;
    case 2:
      this->handle = value;
      break;
    case 3:
      this->enable = value != 0;
      break;
    default:
      return false;
  }
  return true;
}
void BluetoothGATTNotifyDataResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  proto_encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 3, this->data_ptr_, this->data_len_);
}
uint32_t BluetoothGATTNotifyDataResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_uint32(1, this->handle);
  size += ProtoSize::calc_length(1, this->data_len_);
  return size;
}
void BluetoothConnectionsFreeResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->free);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->limit);
  for (const auto &it : this->allocated) {
    if (it != 0) {
      proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 3, it, true);
    }
  }
}
uint32_t BluetoothConnectionsFreeResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->free);
  size += ProtoSize::calc_uint32(1, this->limit);
  for (const auto &it : this->allocated) {
    if (it != 0) {
      size += ProtoSize::calc_uint64_force(1, it);
    }
  }
  return size;
}
void BluetoothGATTErrorResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  proto_encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->error);
}
uint32_t BluetoothGATTErrorResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_uint32(1, this->handle);
  size += ProtoSize::calc_int32(1, this->error);
  return size;
}
void BluetoothGATTWriteResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
}
uint32_t BluetoothGATTWriteResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_uint32(1, this->handle);
  return size;
}
void BluetoothGATTNotifyResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
}
uint32_t BluetoothGATTNotifyResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_uint32(1, this->handle);
  return size;
}
void BluetoothDevicePairingResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->paired);
  proto_encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->error);
}
uint32_t BluetoothDevicePairingResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_bool(1, this->paired);
  size += ProtoSize::calc_int32(1, this->error);
  return size;
}
void BluetoothDeviceUnpairingResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->success);
  proto_encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->error);
}
uint32_t BluetoothDeviceUnpairingResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_bool(1, this->success);
  size += ProtoSize::calc_int32(1, this->error);
  return size;
}
void BluetoothDeviceClearCacheResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->success);
  proto_encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->error);
}
uint32_t BluetoothDeviceClearCacheResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_bool(1, this->success);
  size += ProtoSize::calc_int32(1, this->error);
  return size;
}
void BluetoothScannerStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, static_cast<uint32_t>(this->state));
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->mode));
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, static_cast<uint32_t>(this->configured_mode));
}
uint32_t BluetoothScannerStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->state));
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->mode));
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->configured_mode));
  return size;
}
bool BluetoothScannerSetModeRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->mode = static_cast<enums::BluetoothScannerMode>(value);
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_VOICE_ASSISTANT
bool SubscribeVoiceAssistantRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->subscribe = value != 0;
      break;
    case 2:
      this->flags = value;
      break;
    default:
      return false;
  }
  return true;
}
void VoiceAssistantAudioSettings::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->noise_suppression_level);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->auto_gain);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->volume_multiplier);
}
uint32_t VoiceAssistantAudioSettings::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->noise_suppression_level);
  size += ProtoSize::calc_uint32(1, this->auto_gain);
  size += ProtoSize::calc_float(1, this->volume_multiplier);
  return size;
}
void VoiceAssistantRequest::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 1, this->start);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->conversation_id);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->flags);
  proto_encode_optional_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 4, this->audio_settings);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->wake_word_phrase);
}
uint32_t VoiceAssistantRequest::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_bool(1, this->start);
  size += ProtoSize::calc_length(1, this->conversation_id.size());
  size += ProtoSize::calc_uint32(1, this->flags);
  size += ProtoSize::calc_message(1, this->audio_settings.calculate_size());
  size += ProtoSize::calc_length(1, this->wake_word_phrase.size());
  return size;
}
bool VoiceAssistantResponse::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->port = value;
      break;
    case 2:
      this->error = value != 0;
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantEventData::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->name = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 2: {
      this->value = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantEventResponse::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->event_type = static_cast<enums::VoiceAssistantEvent>(value);
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
bool VoiceAssistantAudio::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->end = value != 0;
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantAudio::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->data = value.data();
      this->data_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
void VoiceAssistantAudio::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 1, this->data, this->data_len);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->end);
}
uint32_t VoiceAssistantAudio::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->data_len);
  size += ProtoSize::calc_bool(1, this->end);
  return size;
}
bool VoiceAssistantTimerEventResponse::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->event_type = static_cast<enums::VoiceAssistantTimerEvent>(value);
      break;
    case 4:
      this->total_seconds = value;
      break;
    case 5:
      this->seconds_left = value;
      break;
    case 6:
      this->is_active = value != 0;
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantTimerEventResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->timer_id = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 3: {
      this->name = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantAnnounceRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 4:
      this->start_conversation = value != 0;
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantAnnounceRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->media_id = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 2: {
      this->text = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 3: {
      this->preannounce_media_id = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    default:
      return false;
  }
  return true;
}
void VoiceAssistantAnnounceFinished::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 1, this->success);
}
uint32_t VoiceAssistantAnnounceFinished::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_bool(1, this->success);
  return size;
}
void VoiceAssistantWakeWord::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->id);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->wake_word);
  for (auto &it : this->trained_languages) {
    proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, it, true);
  }
}
uint32_t VoiceAssistantWakeWord::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->id.size());
  size += ProtoSize::calc_length(1, this->wake_word.size());
  if (!this->trained_languages.empty()) {
    for (const auto &it : this->trained_languages) {
      size += ProtoSize::calc_length_force(1, it.size());
    }
  }
  return size;
}
bool VoiceAssistantExternalWakeWord::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 5:
      this->model_size = value;
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantExternalWakeWord::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->id = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 2: {
      this->wake_word = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 3:
      this->trained_languages.push_back(value.as_string());
      break;
    case 4: {
      this->model_type = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 6: {
      this->model_hash = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
    case 7: {
      this->url = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
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
void VoiceAssistantConfigurationResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  for (auto &it : this->available_wake_words) {
    proto_encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, 1, it);
  }
  for (const auto &it : *this->active_wake_words) {
    proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, it, true);
  }
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->max_active_wake_words);
}
uint32_t VoiceAssistantConfigurationResponse::calculate_size() const {
  uint32_t size = 0;
  if (!this->available_wake_words.empty()) {
    for (const auto &it : this->available_wake_words) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  if (!this->active_wake_words->empty()) {
    for (const auto &it : *this->active_wake_words) {
      size += ProtoSize::calc_length_force(1, it.size());
    }
  }
  size += ProtoSize::calc_uint32(1, this->max_active_wake_words);
  return size;
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
void ListEntitiesAlarmControlPanelResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->supported_features);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->requires_code);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 10, this->requires_code_to_arm);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, this->device_id);
#endif
}
uint32_t ListEntitiesAlarmControlPanelResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_uint32(1, this->supported_features);
  size += ProtoSize::calc_bool(1, this->requires_code);
  size += ProtoSize::calc_bool(1, this->requires_code_to_arm);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void AlarmControlPanelStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->state));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->device_id);
#endif
}
uint32_t AlarmControlPanelStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->state));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool AlarmControlPanelCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->command = static_cast<enums::AlarmControlPanelStateCommand>(value);
      break;
#ifdef USE_DEVICES
    case 4:
      this->device_id = value;
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool AlarmControlPanelCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3: {
      this->code = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
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
void ListEntitiesTextResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->min_length);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->max_length);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, this->pattern);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(this->mode));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, this->device_id);
#endif
}
uint32_t ListEntitiesTextResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_uint32(1, this->min_length);
  size += ProtoSize::calc_uint32(1, this->max_length);
  size += ProtoSize::calc_length(1, this->pattern.size());
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->mode));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void TextStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
}
uint32_t TextStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_length(1, this->state.size());
  size += ProtoSize::calc_bool(1, this->missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool TextCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
#ifdef USE_DEVICES
    case 3:
      this->device_id = value;
      break;
#endif
    default:
      return false;
  }
  return true;
}
bool TextCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->state = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    }
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
void ListEntitiesDateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_id);
#endif
}
uint32_t ListEntitiesDateResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void DateStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->missing_state);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->year);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->month);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->day);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->device_id);
#endif
}
uint32_t DateStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, this->missing_state);
  size += ProtoSize::calc_uint32(1, this->year);
  size += ProtoSize::calc_uint32(1, this->month);
  size += ProtoSize::calc_uint32(1, this->day);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool DateCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->year = value;
      break;
    case 3:
      this->month = value;
      break;
    case 4:
      this->day = value;
      break;
#ifdef USE_DEVICES
    case 5:
      this->device_id = value;
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
void ListEntitiesTimeResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_id);
#endif
}
uint32_t ListEntitiesTimeResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void TimeStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->missing_state);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->hour);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->minute);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->second);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->device_id);
#endif
}
uint32_t TimeStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, this->missing_state);
  size += ProtoSize::calc_uint32(1, this->hour);
  size += ProtoSize::calc_uint32(1, this->minute);
  size += ProtoSize::calc_uint32(1, this->second);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool TimeCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->hour = value;
      break;
    case 3:
      this->minute = value;
      break;
    case 4:
      this->second = value;
      break;
#ifdef USE_DEVICES
    case 5:
      this->device_id = value;
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
void ListEntitiesEventResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
  for (const char *it : *this->event_types) {
    proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, it, strlen(it), true);
  }
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, this->device_id);
#endif
}
uint32_t ListEntitiesEventResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_length(1, this->device_class.size());
  if (!this->event_types->empty()) {
    for (const char *it : *this->event_types) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void EventResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->event_type);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->device_id);
#endif
}
uint32_t EventResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_length(1, this->event_type.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
#endif
#ifdef USE_VALVE
void ListEntitiesValveResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->assumed_state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 10, this->supports_position);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 11, this->supports_stop);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, this->device_id);
#endif
}
uint32_t ListEntitiesValveResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_length(1, this->device_class.size());
  size += ProtoSize::calc_bool(1, this->assumed_state);
  size += ProtoSize::calc_bool(1, this->supports_position);
  size += ProtoSize::calc_bool(1, this->supports_stop);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void ValveStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 2, this->position);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, static_cast<uint32_t>(this->current_operation));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
}
uint32_t ValveStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, this->position);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->current_operation));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool ValveCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->has_position = value != 0;
      break;
    case 4:
      this->stop = value != 0;
      break;
#ifdef USE_DEVICES
    case 5:
      this->device_id = value;
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
void ListEntitiesDateTimeResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_id);
#endif
}
uint32_t ListEntitiesDateTimeResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void DateTimeStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->missing_state);
  proto_encode_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->epoch_seconds);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
}
uint32_t DateTimeStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, this->missing_state);
  size += ProtoSize::calc_fixed32(1, this->epoch_seconds);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool DateTimeCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
#ifdef USE_DEVICES
    case 3:
      this->device_id = value;
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
void ListEntitiesUpdateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_id);
#endif
}
uint32_t ListEntitiesUpdateResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
  size += ProtoSize::calc_length(1, this->device_class.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
void UpdateStateResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->missing_state);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->in_progress);
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 4, this->has_progress);
  proto_encode_float(pos PROTO_ENCODE_DEBUG_ARG, 5, this->progress);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, this->current_version);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 7, this->latest_version);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->title);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, this->release_summary);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, this->release_url);
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, this->device_id);
#endif
}
uint32_t UpdateStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, this->missing_state);
  size += ProtoSize::calc_bool(1, this->in_progress);
  size += ProtoSize::calc_bool(1, this->has_progress);
  size += ProtoSize::calc_float(1, this->progress);
  size += ProtoSize::calc_length(1, this->current_version.size());
  size += ProtoSize::calc_length(1, this->latest_version.size());
  size += ProtoSize::calc_length(1, this->title.size());
  size += ProtoSize::calc_length(1, this->release_summary.size());
  size += ProtoSize::calc_length(1, this->release_url.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
bool UpdateCommandRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 2:
      this->command = static_cast<enums::UpdateCommand>(value);
      break;
#ifdef USE_DEVICES
    case 3:
      this->device_id = value;
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
      this->data = value.data();
      this->data_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
void ZWaveProxyFrame::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 1, this->data, this->data_len);
}
uint32_t ZWaveProxyFrame::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->data_len);
  return size;
}
bool ZWaveProxyRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->type = static_cast<enums::ZWaveProxyRequestType>(value);
      break;
    default:
      return false;
  }
  return true;
}
bool ZWaveProxyRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->data = value.data();
      this->data_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
void ZWaveProxyRequest::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, static_cast<uint32_t>(this->type));
  proto_encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 2, this->data, this->data_len);
}
uint32_t ZWaveProxyRequest::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->type));
  size += ProtoSize::calc_length(1, this->data_len);
  return size;
}
#endif
#ifdef USE_INFRARED
void ListEntitiesInfraredResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->object_id);
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->name);
#ifdef USE_ENTITY_ICON
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, this->icon);
#endif
  proto_encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->disabled_by_default);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, this->device_id);
#endif
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->capabilities);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->receiver_frequency);
}
uint32_t ListEntitiesInfraredResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->object_id.size());
  size += 5;
  size += ProtoSize::calc_length(1, this->name.size());
#ifdef USE_ENTITY_ICON
  size += ProtoSize::calc_length(1, this->icon.size());
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  size += ProtoSize::calc_uint32(1, this->capabilities);
  size += ProtoSize::calc_uint32(1, this->receiver_frequency);
  return size;
}
#endif
#ifdef USE_IR_RF
bool InfraredRFTransmitRawTimingsRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
#ifdef USE_DEVICES
    case 1:
      this->device_id = value;
      break;
#endif
    case 3:
      this->carrier_frequency = value;
      break;
    case 4:
      this->repeat_count = value;
      break;
    default:
      return false;
  }
  return true;
}
bool InfraredRFTransmitRawTimingsRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 5: {
      this->timings_data_ = value.data();
      this->timings_length_ = value.size();
      this->timings_count_ = count_packed_varints(value.data(), value.size());
      break;
    }
    default:
      return false;
  }
  return true;
}
bool InfraredRFTransmitRawTimingsRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2:
      this->key = value.as_fixed32();
      break;
    default:
      return false;
  }
  return true;
}
void InfraredRFReceiveEvent::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
#ifdef USE_DEVICES
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->device_id);
#endif
  proto_write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  for (const auto &it : *this->timings) {
    proto_encode_sint32(pos PROTO_ENCODE_DEBUG_ARG, 3, it, true);
  }
}
uint32_t InfraredRFReceiveEvent::calculate_size() const {
  uint32_t size = 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  size += 5;
  if (!this->timings->empty()) {
    for (const auto &it : *this->timings) {
      size += ProtoSize::calc_sint32_force(1, it);
    }
  }
  return size;
}
#endif
#ifdef USE_SERIAL_PROXY
bool SerialProxyConfigureRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->instance = value;
      break;
    case 2:
      this->baudrate = value;
      break;
    case 3:
      this->flow_control = value != 0;
      break;
    case 4:
      this->parity = static_cast<enums::SerialProxyParity>(value);
      break;
    case 5:
      this->stop_bits = value;
      break;
    case 6:
      this->data_size = value;
      break;
    default:
      return false;
  }
  return true;
}
void SerialProxyDataReceived::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->instance);
  proto_encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 2, this->data_ptr_, this->data_len_);
}
uint32_t SerialProxyDataReceived::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->instance);
  size += ProtoSize::calc_length(1, this->data_len_);
  return size;
}
bool SerialProxyWriteRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->instance = value;
      break;
    default:
      return false;
  }
  return true;
}
bool SerialProxyWriteRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->data = value.data();
      this->data_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
bool SerialProxySetModemPinsRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->instance = value;
      break;
    case 2:
      this->line_states = value;
      break;
    default:
      return false;
  }
  return true;
}
bool SerialProxyGetModemPinsRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->instance = value;
      break;
    default:
      return false;
  }
  return true;
}
void SerialProxyGetModemPinsResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->instance);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->line_states);
}
uint32_t SerialProxyGetModemPinsResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->instance);
  size += ProtoSize::calc_uint32(1, this->line_states);
  return size;
}
bool SerialProxyRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->instance = value;
      break;
    case 2:
      this->type = static_cast<enums::SerialProxyRequestType>(value);
      break;
    default:
      return false;
  }
  return true;
}
void SerialProxyRequestResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->instance);
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->type));
  proto_encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, static_cast<uint32_t>(this->status));
  proto_encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, this->error_message);
}
uint32_t SerialProxyRequestResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->instance);
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->type));
  size += ProtoSize::calc_uint32(1, static_cast<uint32_t>(this->status));
  size += ProtoSize::calc_length(1, this->error_message.size());
  return size;
}
#endif
#ifdef USE_BLUETOOTH_PROXY
bool BluetoothSetConnectionParamsRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->address = value;
      break;
    case 2:
      this->min_interval = value;
      break;
    case 3:
      this->max_interval = value;
      break;
    case 4:
      this->latency = value;
      break;
    case 5:
      this->timeout = value;
      break;
    default:
      return false;
  }
  return true;
}
void BluetoothSetConnectionParamsResponse::encode(uint8_t *__restrict__ &pos PROTO_ENCODE_DEBUG_PARAM) const {
  proto_encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  proto_encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->error);
}
uint32_t BluetoothSetConnectionParamsResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_int32(1, this->error);
  return size;
}
#endif

}  // namespace esphome::api
