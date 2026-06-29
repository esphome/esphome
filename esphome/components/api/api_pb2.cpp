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
uint8_t *HelloResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->api_version_major);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->api_version_minor);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->server_info);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 34, this->name);
  return pos;
}
uint32_t HelloResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->api_version_major);
  size += ProtoSize::calc_uint32(1, this->api_version_minor);
  size += 2 + this->server_info.size();
  size += 2 + this->name.size();
  return size;
}
#ifdef USE_AREAS
uint8_t *AreaInfo::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->area_id);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 18, this->name);
  return pos;
}
uint32_t AreaInfo::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->area_id);
  size += 2 + this->name.size();
  return size;
}
#endif
#ifdef USE_DEVICES
uint8_t *DeviceInfo::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->device_id);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 18, this->name);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->area_id);
  return pos;
}
uint32_t DeviceInfo::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->device_id);
  size += 2 + this->name.size();
  size += ProtoSize::calc_uint32(1, this->area_id);
  return size;
}
#endif
#ifdef USE_SERIAL_PROXY
uint8_t *SerialProxyInfo::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->name);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->port_type));
  return pos;
}
uint32_t SerialProxyInfo::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->name.size());
  size += this->port_type ? 2 : 0;
  return size;
}
#endif
uint8_t *DeviceInfoResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 18, this->name);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->mac_address);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 34, this->esphome_version);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 42, this->compilation_time);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 50, this->model);
#ifdef USE_DEEP_SLEEP
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->has_deep_sleep);
#endif
#ifdef ESPHOME_PROJECT_NAME
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 66, this->project_name);
#endif
#ifdef ESPHOME_PROJECT_NAME
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 74, this->project_version);
#endif
#ifdef USE_WEBSERVER
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, this->webserver_port);
#endif
#ifdef USE_BLUETOOTH_PROXY
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 15, this->bluetooth_proxy_feature_flags);
#endif
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 98, this->manufacturer);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 106, this->friendly_name);
#ifdef USE_VOICE_ASSISTANT
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 17, this->voice_assistant_feature_flags);
#endif
#ifdef USE_AREAS
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 16, this->suggested_area, true);
#endif
#ifdef USE_BLUETOOTH_PROXY
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 18, this->bluetooth_mac_address, true);
#endif
#ifdef USE_API_NOISE
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 19, this->api_encryption_supported);
#endif
#ifdef USE_DEVICES
  for (const auto &it : this->devices) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 20, it);
  }
#endif
#ifdef USE_AREAS
  for (const auto &it : this->areas) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 21, it);
  }
#endif
#ifdef USE_AREAS
  ProtoEncode::encode_optional_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 22, this->area);
#endif
#ifdef USE_ZWAVE_PROXY
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 23, this->zwave_proxy_feature_flags);
#endif
#ifdef USE_ZWAVE_PROXY
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 24, this->zwave_home_id);
#endif
#ifdef USE_SERIAL_PROXY
  for (const auto &it : this->serial_proxies) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 25, it);
  }
#endif
  return pos;
}
uint32_t DeviceInfoResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->name.size();
  size += 2 + this->mac_address.size();
  size += 2 + this->esphome_version.size();
  size += 2 + this->compilation_time.size();
  size += 2 + this->model.size();
#ifdef USE_DEEP_SLEEP
  size += ProtoSize::calc_bool(1, this->has_deep_sleep);
#endif
#ifdef ESPHOME_PROJECT_NAME
  size += 2 + this->project_name.size();
#endif
#ifdef ESPHOME_PROJECT_NAME
  size += 2 + this->project_version.size();
#endif
#ifdef USE_WEBSERVER
  size += ProtoSize::calc_uint32(1, this->webserver_port);
#endif
#ifdef USE_BLUETOOTH_PROXY
  size += ProtoSize::calc_uint32(1, this->bluetooth_proxy_feature_flags);
#endif
  size += 2 + this->manufacturer.size();
  size += 2 + this->friendly_name.size();
#ifdef USE_VOICE_ASSISTANT
  size += ProtoSize::calc_uint32(2, this->voice_assistant_feature_flags);
#endif
#ifdef USE_AREAS
  size += 3 + this->suggested_area.size();
#endif
#ifdef USE_BLUETOOTH_PROXY
  size += 3 + this->bluetooth_mac_address.size();
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
uint8_t *ListEntitiesBinarySensorResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->device_class);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->is_status_binary_sensor);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->icon);
#endif
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesBinarySensorResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
  size += !this->device_class.empty() ? 2 + this->device_class.size() : 0;
  size += ProtoSize::calc_bool(1, this->is_status_binary_sensor);
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *BinarySensorStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
  return pos;
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
uint8_t *ListEntitiesCoverResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->assumed_state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->supports_position);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->supports_tilt);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, this->icon);
#endif
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 12, this->supports_stop);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesCoverResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
  size += ProtoSize::calc_bool(1, this->assumed_state);
  size += ProtoSize::calc_bool(1, this->supports_position);
  size += ProtoSize::calc_bool(1, this->supports_tilt);
  size += !this->device_class.empty() ? 2 + this->device_class.size() : 0;
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += this->entity_category ? 2 : 0;
  size += ProtoSize::calc_bool(1, this->supports_stop);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *CoverStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->position);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 4, this->tilt);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, static_cast<uint32_t>(this->current_operation));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->device_id);
#endif
  return pos;
}
uint32_t CoverStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, this->position);
  size += ProtoSize::calc_float(1, this->tilt);
  size += this->current_operation ? 2 : 0;
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
uint8_t *ListEntitiesFanResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->supports_oscillation);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->supports_speed);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->supports_direction);
  ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->supported_speed_count);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, this->icon);
#endif
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(this->entity_category));
  for (const char *it : *this->supported_preset_modes) {
    ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 12, it, strlen(it), true);
  }
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesFanResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
  size += ProtoSize::calc_bool(1, this->supports_oscillation);
  size += ProtoSize::calc_bool(1, this->supports_speed);
  size += ProtoSize::calc_bool(1, this->supports_direction);
  size += ProtoSize::calc_int32(1, this->supported_speed_count);
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += this->entity_category ? 2 : 0;
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
uint8_t *FanStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->oscillating);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, static_cast<uint32_t>(this->direction));
  ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->speed_level);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 7, this->preset_mode);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_id);
#endif
  return pos;
}
uint32_t FanStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, this->state);
  size += ProtoSize::calc_bool(1, this->oscillating);
  size += this->direction ? 2 : 0;
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
uint8_t *ListEntitiesLightResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
  for (const auto &it : *this->supported_color_modes) {
    ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, static_cast<uint32_t>(it), true);
  }
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 9, this->min_mireds);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 10, this->max_mireds);
  for (const char *it : *this->effects) {
    ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 11, it, strlen(it), true);
  }
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 13, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 14, this->icon);
#endif
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 15, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 16, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesLightResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
  if (!this->supported_color_modes->empty()) {
    size += this->supported_color_modes->size() * 2;
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
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(2, this->device_id);
#endif
  return size;
}
uint8_t *LightStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->brightness);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(this->color_mode));
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 10, this->color_brightness);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 4, this->red);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 5, this->green);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 6, this->blue);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 7, this->white);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 8, this->color_temperature);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 12, this->cold_white);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 13, this->warm_white);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, this->effect);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 14, this->device_id);
#endif
  return pos;
}
uint32_t LightStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, this->state);
  size += ProtoSize::calc_float(1, this->brightness);
  size += this->color_mode ? 2 : 0;
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
uint8_t *ListEntitiesSensorResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, this->unit_of_measurement);
  ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 7, this->accuracy_decimals);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 8, this->force_update);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_class);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(this->state_class));
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 12, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 14, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesSensorResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += !this->unit_of_measurement.empty() ? 2 + this->unit_of_measurement.size() : 0;
  size += ProtoSize::calc_int32(1, this->accuracy_decimals);
  size += ProtoSize::calc_bool(1, this->force_update);
  size += !this->device_class.empty() ? 2 + this->device_class.size() : 0;
  size += this->state_class ? 2 : 0;
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
SensorStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
SensorStateResponse::calculate_size() const {
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
uint8_t *ListEntitiesSwitchResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->assumed_state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_class);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesSwitchResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->assumed_state);
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
  size += !this->device_class.empty() ? 2 + this->device_class.size() : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *SwitchStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->device_id);
#endif
  return pos;
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
uint8_t *ListEntitiesTextSensorResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesTextSensorResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
  size += !this->device_class.empty() ? 2 + this->device_class.size() : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *TextSensorStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
  return pos;
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
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
SubscribeLogsResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, static_cast<uint32_t>(this->level), true);
  ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 26);
  ProtoEncode::encode_varint_raw(pos PROTO_ENCODE_DEBUG_ARG, this->message_len_);
  ProtoEncode::encode_raw(pos PROTO_ENCODE_DEBUG_ARG, this->message_ptr_, this->message_len_);
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
SubscribeLogsResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2;
  size += ProtoSize::calc_length_force(1, this->message_len_);
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
uint8_t *NoiseEncryptionSetKeyResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 1, this->success);
  return pos;
}
uint32_t NoiseEncryptionSetKeyResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_bool(1, this->success);
  return size;
}
#endif
#ifdef USE_API_HOMEASSISTANT_SERVICES
uint8_t *HomeassistantServiceMap::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->key);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->value);
  return pos;
}
uint32_t HomeassistantServiceMap::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->key.size());
  size += ProtoSize::calc_length(1, this->value.size());
  return size;
}
uint8_t *HomeassistantActionRequest::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->service);
  for (auto &it : this->data) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 2, it);
  }
  for (auto &it : this->data_template) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 3, it);
  }
  for (auto &it : this->variables) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 4, it);
  }
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->is_event);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->call_id);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->wants_response);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->response_template);
#endif
  return pos;
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
uint8_t *SubscribeHomeAssistantStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->entity_id);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->attribute);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->once);
  return pos;
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
uint8_t *ListEntitiesServicesArgument::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->name);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->type));
  return pos;
}
uint32_t ListEntitiesServicesArgument::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->name.size());
  size += this->type ? 2 : 0;
  return size;
}
uint8_t *ListEntitiesServicesResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->name);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  for (auto &it : this->args) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 3, it);
  }
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, static_cast<uint32_t>(this->supports_response));
  return pos;
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
  size += this->supports_response ? 2 : 0;
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
uint8_t *ExecuteServiceResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->call_id);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->success);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, this->error_message);
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 4, this->response_data, this->response_data_len);
#endif
  return pos;
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
uint8_t *ListEntitiesCameraResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, this->icon);
#endif
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesCameraResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *CameraImageResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 2, this->data_ptr_, this->data_len_);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->done);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
  return pos;
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
uint8_t *ListEntitiesClimateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->supports_current_temperature);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->supports_two_point_target_temperature);
  for (const auto &it : *this->supported_modes) {
    ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(it), true);
  }
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 8, this->visual_min_temperature);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 9, this->visual_max_temperature);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 10, this->visual_target_temperature_step);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 12, this->supports_action);
  for (const auto &it : *this->supported_fan_modes) {
    ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, static_cast<uint32_t>(it), true);
  }
  for (const auto &it : *this->supported_swing_modes) {
    ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 14, static_cast<uint32_t>(it), true);
  }
  for (const char *it : *this->supported_custom_fan_modes) {
    ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 15, it, strlen(it), true);
  }
  for (const auto &it : *this->supported_presets) {
    ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 16, static_cast<uint32_t>(it), true);
  }
  for (const char *it : *this->supported_custom_presets) {
    ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 17, it, strlen(it), true);
  }
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 18, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 19, this->icon);
#endif
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 20, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 21, this->visual_current_temperature_step);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 22, this->supports_current_humidity);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 23, this->supports_target_humidity);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 24, this->visual_min_humidity);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 25, this->visual_max_humidity);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 26, this->device_id);
#endif
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 27, this->feature_flags);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 28, static_cast<uint32_t>(this->temperature_unit));
  return pos;
}
uint32_t ListEntitiesClimateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
  size += ProtoSize::calc_bool(1, this->supports_current_temperature);
  size += ProtoSize::calc_bool(1, this->supports_two_point_target_temperature);
  if (!this->supported_modes->empty()) {
    size += this->supported_modes->size() * 2;
  }
  size += ProtoSize::calc_float(1, this->visual_min_temperature);
  size += ProtoSize::calc_float(1, this->visual_max_temperature);
  size += ProtoSize::calc_float(1, this->visual_target_temperature_step);
  size += ProtoSize::calc_bool(1, this->supports_action);
  if (!this->supported_fan_modes->empty()) {
    size += this->supported_fan_modes->size() * 2;
  }
  if (!this->supported_swing_modes->empty()) {
    size += this->supported_swing_modes->size() * 2;
  }
  if (!this->supported_custom_fan_modes->empty()) {
    for (const char *it : *this->supported_custom_fan_modes) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
  if (!this->supported_presets->empty()) {
    size += this->supported_presets->size() * 3;
  }
  if (!this->supported_custom_presets->empty()) {
    for (const char *it : *this->supported_custom_presets) {
      size += ProtoSize::calc_length_force(2, strlen(it));
    }
  }
  size += ProtoSize::calc_bool(2, this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 3 + this->icon.size() : 0;
#endif
  size += this->entity_category ? 3 : 0;
  size += ProtoSize::calc_float(2, this->visual_current_temperature_step);
  size += ProtoSize::calc_bool(2, this->supports_current_humidity);
  size += ProtoSize::calc_bool(2, this->supports_target_humidity);
  size += ProtoSize::calc_float(2, this->visual_min_humidity);
  size += ProtoSize::calc_float(2, this->visual_max_humidity);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(2, this->device_id);
#endif
  size += ProtoSize::calc_uint32(2, this->feature_flags);
  size += this->temperature_unit ? 3 : 0;
  return size;
}
uint8_t *ClimateStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->mode));
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->current_temperature);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 4, this->target_temperature);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 5, this->target_temperature_low);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 6, this->target_temperature_high);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, static_cast<uint32_t>(this->action));
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, static_cast<uint32_t>(this->fan_mode));
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(this->swing_mode));
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 11, this->custom_fan_mode);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, static_cast<uint32_t>(this->preset));
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 13, this->custom_preset);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 14, this->current_humidity);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 15, this->target_humidity);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 16, this->device_id);
#endif
  return pos;
}
uint32_t ClimateStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += this->mode ? 2 : 0;
  size += ProtoSize::calc_float(1, this->current_temperature);
  size += ProtoSize::calc_float(1, this->target_temperature);
  size += ProtoSize::calc_float(1, this->target_temperature_low);
  size += ProtoSize::calc_float(1, this->target_temperature_high);
  size += this->action ? 2 : 0;
  size += this->fan_mode ? 2 : 0;
  size += this->swing_mode ? 2 : 0;
  size += ProtoSize::calc_length(1, this->custom_fan_mode.size());
  size += this->preset ? 2 : 0;
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
uint8_t *ListEntitiesWaterHeaterResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, this->device_id);
#endif
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 8, this->min_temperature);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 9, this->max_temperature);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 10, this->target_temperature_step);
  for (const auto &it : *this->supported_modes) {
    ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(it), true);
  }
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, this->supported_features);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, static_cast<uint32_t>(this->temperature_unit));
  return pos;
}
uint32_t ListEntitiesWaterHeaterResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  size += ProtoSize::calc_float(1, this->min_temperature);
  size += ProtoSize::calc_float(1, this->max_temperature);
  size += ProtoSize::calc_float(1, this->target_temperature_step);
  if (!this->supported_modes->empty()) {
    size += this->supported_modes->size() * 2;
  }
  size += ProtoSize::calc_uint32(1, this->supported_features);
  size += this->temperature_unit ? 2 : 0;
  return size;
}
uint8_t *WaterHeaterStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 2, this->current_temperature);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->target_temperature);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, static_cast<uint32_t>(this->mode));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->device_id);
#endif
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->state);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 7, this->target_temperature_low);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 8, this->target_temperature_high);
  return pos;
}
uint32_t WaterHeaterStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, this->current_temperature);
  size += ProtoSize::calc_float(1, this->target_temperature);
  size += this->mode ? 2 : 0;
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
uint8_t *ListEntitiesNumberResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 6, this->min_value);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 7, this->max_value);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 8, this->step);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 11, this->unit_of_measurement);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, static_cast<uint32_t>(this->mode));
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 13, this->device_class);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 14, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesNumberResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_float(1, this->min_value);
  size += ProtoSize::calc_float(1, this->max_value);
  size += ProtoSize::calc_float(1, this->step);
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
  size += !this->unit_of_measurement.empty() ? 2 + this->unit_of_measurement.size() : 0;
  size += this->mode ? 2 : 0;
  size += !this->device_class.empty() ? 2 + this->device_class.size() : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *NumberStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
  return pos;
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
uint8_t *ListEntitiesSelectResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  for (const char *it : *this->options) {
    ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, it, strlen(it), true);
  }
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesSelectResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  if (!this->options->empty()) {
    for (const char *it : *this->options) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *SelectStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
  return pos;
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
uint8_t *ListEntitiesSirenResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  for (const char *it : *this->tones) {
    ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 7, it, strlen(it), true);
  }
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 8, this->supports_duration);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->supports_volume);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesSirenResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  if (!this->tones->empty()) {
    for (const char *it : *this->tones) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
  size += ProtoSize::calc_bool(1, this->supports_duration);
  size += ProtoSize::calc_bool(1, this->supports_volume);
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *SirenStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->device_id);
#endif
  return pos;
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
uint8_t *ListEntitiesLockResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 8, this->assumed_state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->supports_open);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 10, this->requires_code);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 11, this->code_format);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesLockResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
  size += ProtoSize::calc_bool(1, this->assumed_state);
  size += ProtoSize::calc_bool(1, this->supports_open);
  size += ProtoSize::calc_bool(1, this->requires_code);
  size += ProtoSize::calc_length(1, this->code_format.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *LockStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->state));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->device_id);
#endif
  return pos;
}
uint32_t LockStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += this->state ? 2 : 0;
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
uint8_t *ListEntitiesButtonResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesButtonResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
  size += !this->device_class.empty() ? 2 + this->device_class.size() : 0;
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
uint8_t *MediaPlayerSupportedFormat::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->format);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->sample_rate);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->num_channels);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, static_cast<uint32_t>(this->purpose));
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->sample_bytes);
  return pos;
}
uint32_t MediaPlayerSupportedFormat::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->format.size());
  size += ProtoSize::calc_uint32(1, this->sample_rate);
  size += ProtoSize::calc_uint32(1, this->num_channels);
  size += this->purpose ? 2 : 0;
  size += ProtoSize::calc_uint32(1, this->sample_bytes);
  return size;
}
uint8_t *ListEntitiesMediaPlayerResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 8, this->supports_pause);
  for (auto &it : this->supported_formats) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 9, it);
  }
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, this->device_id);
#endif
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, this->feature_flags);
  return pos;
}
uint32_t ListEntitiesMediaPlayerResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
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
uint8_t *MediaPlayerStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->state));
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->volume);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 4, this->muted);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->device_id);
#endif
  return pos;
}
uint32_t MediaPlayerStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += this->state ? 2 : 0;
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
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
BluetoothLERawAdvertisementsResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  for (uint16_t i = 0; i < this->advertisements_len; i++) {
    auto &sub_msg = this->advertisements[i];
    ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 10);
    uint8_t *len_pos = pos;
    ProtoEncode::reserve_byte(pos PROTO_ENCODE_DEBUG_ARG);
    ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 8);
    ProtoEncode::encode_varint_raw_48bit(pos PROTO_ENCODE_DEBUG_ARG, sub_msg.address);
    ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 16);
    ProtoEncode::encode_varint_raw_short(pos PROTO_ENCODE_DEBUG_ARG, encode_zigzag32(sub_msg.rssi));
    if (sub_msg.address_type) {
      ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 24);
      ProtoEncode::encode_varint_raw(pos PROTO_ENCODE_DEBUG_ARG, sub_msg.address_type);
    }
    ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 34);
    ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, static_cast<uint8_t>(sub_msg.data_len));
    ProtoEncode::encode_raw(pos PROTO_ENCODE_DEBUG_ARG, sub_msg.data, sub_msg.data_len);
    *len_pos = static_cast<uint8_t>(pos - len_pos - 1);
  }
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
BluetoothLERawAdvertisementsResponse::calculate_size() const {
  uint32_t size = 0;
  for (uint16_t i = 0; i < this->advertisements_len; i++) {
    auto &sub_msg = this->advertisements[i];
    size += 2;
    size += ProtoSize::calc_uint64_48bit_force(1, sub_msg.address);
    size += ProtoSize::calc_sint32_force(1, sub_msg.rssi);
    size += sub_msg.address_type ? 2 : 0;
    size += 2 + sub_msg.data_len;
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
uint8_t *BluetoothDeviceConnectionResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->connected);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->mtu);
  ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->error);
  return pos;
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
uint8_t *BluetoothGATTDescriptor::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[0], true);
    ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[1], true);
  }
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->short_uuid);
  return pos;
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
uint8_t *BluetoothGATTCharacteristic::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[0], true);
    ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[1], true);
  }
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->properties);
  for (auto &it : this->descriptors) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 4, it);
  }
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->short_uuid);
  return pos;
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
uint8_t *BluetoothGATTService::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  if (this->uuid[0] != 0 || this->uuid[1] != 0) {
    ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[0], true);
    ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->uuid[1], true);
  }
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  for (auto &it : this->characteristics) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 3, it);
  }
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->short_uuid);
  return pos;
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
uint8_t *BluetoothGATTGetServicesResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  for (auto &it : this->services) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 2, it);
  }
  return pos;
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
uint8_t *BluetoothGATTGetServicesDoneResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  return pos;
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
uint8_t *BluetoothGATTReadResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 3, this->data_ptr_, this->data_len_);
  return pos;
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
uint8_t *BluetoothGATTNotifyDataResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 3, this->data_ptr_, this->data_len_);
  return pos;
}
uint32_t BluetoothGATTNotifyDataResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_uint32(1, this->handle);
  size += ProtoSize::calc_length(1, this->data_len_);
  return size;
}
uint8_t *BluetoothConnectionsFreeResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->free);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->limit);
  for (const auto &it : this->allocated) {
    if (it != 0) {
      ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 3, it, true);
    }
  }
  return pos;
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
uint8_t *BluetoothGATTErrorResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->error);
  return pos;
}
uint32_t BluetoothGATTErrorResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_uint32(1, this->handle);
  size += ProtoSize::calc_int32(1, this->error);
  return size;
}
uint8_t *BluetoothGATTWriteResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  return pos;
}
uint32_t BluetoothGATTWriteResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_uint32(1, this->handle);
  return size;
}
uint8_t *BluetoothGATTNotifyResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->handle);
  return pos;
}
uint32_t BluetoothGATTNotifyResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_uint32(1, this->handle);
  return size;
}
uint8_t *BluetoothDevicePairingResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->paired);
  ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->error);
  return pos;
}
uint32_t BluetoothDevicePairingResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_bool(1, this->paired);
  size += ProtoSize::calc_int32(1, this->error);
  return size;
}
uint8_t *BluetoothDeviceUnpairingResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->success);
  ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->error);
  return pos;
}
uint32_t BluetoothDeviceUnpairingResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_bool(1, this->success);
  size += ProtoSize::calc_int32(1, this->error);
  return size;
}
uint8_t *BluetoothDeviceClearCacheResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->success);
  ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->error);
  return pos;
}
uint32_t BluetoothDeviceClearCacheResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_bool(1, this->success);
  size += ProtoSize::calc_int32(1, this->error);
  return size;
}
uint8_t *BluetoothScannerStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, static_cast<uint32_t>(this->state));
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->mode));
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, static_cast<uint32_t>(this->configured_mode));
  return pos;
}
uint32_t BluetoothScannerStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += this->state ? 2 : 0;
  size += this->mode ? 2 : 0;
  size += this->configured_mode ? 2 : 0;
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
uint8_t *VoiceAssistantAudioSettings::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->noise_suppression_level);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->auto_gain);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 3, this->volume_multiplier);
  return pos;
}
uint32_t VoiceAssistantAudioSettings::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->noise_suppression_level);
  size += ProtoSize::calc_uint32(1, this->auto_gain);
  size += ProtoSize::calc_float(1, this->volume_multiplier);
  return size;
}
uint8_t *VoiceAssistantRequest::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 1, this->start);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->conversation_id);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->flags);
  ProtoEncode::encode_optional_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 4, this->audio_settings);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->wake_word_phrase);
  return pos;
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
    case 3: {
      this->data2 = value.data();
      this->data2_len = value.size();
      break;
    }
    default:
      return false;
  }
  return true;
}
uint8_t *VoiceAssistantAudio::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 1, this->data, this->data_len);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->end);
  ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 3, this->data2, this->data2_len);
  return pos;
}
uint32_t VoiceAssistantAudio::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, this->data_len);
  size += ProtoSize::calc_bool(1, this->end);
  size += ProtoSize::calc_length(1, this->data2_len);
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
uint8_t *VoiceAssistantAnnounceFinished::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 1, this->success);
  return pos;
}
uint32_t VoiceAssistantAnnounceFinished::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_bool(1, this->success);
  return size;
}
uint8_t *VoiceAssistantWakeWord::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, this->id);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->wake_word);
  for (auto &it : this->trained_languages) {
    ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, it, true);
  }
  return pos;
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
uint8_t *VoiceAssistantConfigurationResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  for (auto &it : this->available_wake_words) {
    ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 1, it);
  }
  for (const auto &it : *this->active_wake_words) {
    ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, it, true);
  }
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->max_active_wake_words);
  return pos;
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
uint8_t *ListEntitiesAlarmControlPanelResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->supported_features);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->requires_code);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 10, this->requires_code_to_arm);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesAlarmControlPanelResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
  size += ProtoSize::calc_uint32(1, this->supported_features);
  size += ProtoSize::calc_bool(1, this->requires_code);
  size += ProtoSize::calc_bool(1, this->requires_code_to_arm);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *AlarmControlPanelStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->state));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->device_id);
#endif
  return pos;
}
uint32_t AlarmControlPanelStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += this->state ? 2 : 0;
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
uint8_t *ListEntitiesTextResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->min_length);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->max_length);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, this->pattern);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(this->mode));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesTextResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
  size += ProtoSize::calc_uint32(1, this->min_length);
  size += ProtoSize::calc_uint32(1, this->max_length);
  size += ProtoSize::calc_length(1, this->pattern.size());
  size += this->mode ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *TextStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->missing_state);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
  return pos;
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
uint8_t *ListEntitiesDateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesDateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *DateStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->missing_state);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->year);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->month);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->day);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->device_id);
#endif
  return pos;
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
uint8_t *ListEntitiesTimeResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesTimeResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *TimeStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->missing_state);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->hour);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->minute);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, this->second);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, this->device_id);
#endif
  return pos;
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
uint8_t *ListEntitiesEventResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
  for (const char *it : *this->event_types) {
    ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, it, strlen(it), true);
  }
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesEventResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
  size += !this->device_class.empty() ? 2 + this->device_class.size() : 0;
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
uint8_t *EventResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, this->event_type);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->device_id);
#endif
  return pos;
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
uint8_t *ListEntitiesValveResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, this->assumed_state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 10, this->supports_position);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 11, this->supports_stop);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesValveResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
  size += !this->device_class.empty() ? 2 + this->device_class.size() : 0;
  size += ProtoSize::calc_bool(1, this->assumed_state);
  size += ProtoSize::calc_bool(1, this->supports_position);
  size += ProtoSize::calc_bool(1, this->supports_stop);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *ValveStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 2, this->position);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, static_cast<uint32_t>(this->current_operation));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
  return pos;
}
uint32_t ValveStateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, this->position);
  size += this->current_operation ? 2 : 0;
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
uint8_t *ListEntitiesDateTimeResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesDateTimeResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *DateTimeStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->missing_state);
  ProtoEncode::encode_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 3, this->epoch_seconds);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, this->device_id);
#endif
  return pos;
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
uint8_t *ListEntitiesUpdateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(this->entity_category));
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->device_class);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->device_id);
#endif
  return pos;
}
uint32_t ListEntitiesUpdateResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
  size += !this->device_class.empty() ? 2 + this->device_class.size() : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  return size;
}
uint8_t *UpdateStateResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, this->key);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, this->missing_state);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, this->in_progress);
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 4, this->has_progress);
  ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 5, this->progress);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, this->current_version);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 7, this->latest_version);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, this->title);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, this->release_summary);
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, this->release_url);
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, this->device_id);
#endif
  return pos;
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
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
ZWaveProxyFrame::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 1, this->data, this->data_len);
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
ZWaveProxyFrame::calculate_size() const {
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
uint8_t *ZWaveProxyRequest::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, static_cast<uint32_t>(this->type));
  ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 2, this->data, this->data_len);
  return pos;
}
uint32_t ZWaveProxyRequest::calculate_size() const {
  uint32_t size = 0;
  size += this->type ? 2 : 0;
  size += ProtoSize::calc_length(1, this->data_len);
  return size;
}
#endif
#ifdef USE_INFRARED
uint8_t *ListEntitiesInfraredResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, this->device_id);
#endif
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->capabilities);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->receiver_frequency);
  return pos;
}
uint32_t ListEntitiesInfraredResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  size += ProtoSize::calc_uint32(1, this->capabilities);
  size += ProtoSize::calc_uint32(1, this->receiver_frequency);
  return size;
}
#endif
#if defined(USE_IR_RF) || defined(USE_RADIO_FREQUENCY)
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
    case 6:
      this->modulation = value;
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
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
InfraredRFReceiveEvent::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->device_id);
#endif
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  for (const auto &it : *this->timings) {
    ProtoEncode::encode_sint32(pos PROTO_ENCODE_DEBUG_ARG, 3, it, true);
  }
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
InfraredRFReceiveEvent::calculate_size() const {
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
#ifdef USE_RADIO_FREQUENCY
uint8_t *ListEntitiesRadioFrequencyResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, this->object_id);
  ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, this->key);
  ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, this->name);
#ifdef USE_ENTITY_ICON
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, this->icon);
#endif
  ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, this->disabled_by_default);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, static_cast<uint32_t>(this->entity_category));
#ifdef USE_DEVICES
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, this->device_id);
#endif
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, this->capabilities);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, this->frequency_min);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, this->frequency_max);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, this->supported_modulations);
  return pos;
}
uint32_t ListEntitiesRadioFrequencyResponse::calculate_size() const {
  uint32_t size = 0;
  size += 2 + this->object_id.size();
  size += 5;
  size += 2 + this->name.size();
#ifdef USE_ENTITY_ICON
  size += !this->icon.empty() ? 2 + this->icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, this->disabled_by_default);
  size += this->entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, this->device_id);
#endif
  size += ProtoSize::calc_uint32(1, this->capabilities);
  size += ProtoSize::calc_uint32(1, this->frequency_min);
  size += ProtoSize::calc_uint32(1, this->frequency_max);
  size += ProtoSize::calc_uint32(1, this->supported_modulations);
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
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
SerialProxyDataReceived::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->instance);
  ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 2, this->data_ptr_, this->data_len_);
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
SerialProxyDataReceived::calculate_size() const {
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
uint8_t *SerialProxyGetModemPinsResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->instance);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->line_states);
  return pos;
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
uint8_t *SerialProxyRequestResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, this->instance);
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(this->type));
  ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, static_cast<uint32_t>(this->status));
  ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, this->error_message);
  return pos;
}
uint32_t SerialProxyRequestResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, this->instance);
  size += this->type ? 2 : 0;
  size += this->status ? 2 : 0;
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
uint8_t *BluetoothSetConnectionParamsResponse::encode(ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) const {
  uint8_t *__restrict__ pos = buffer.get_pos();
  ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, this->address);
  ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 2, this->error);
  return pos;
}
uint32_t BluetoothSetConnectionParamsResponse::calculate_size() const {
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, this->address);
  size += ProtoSize::calc_int32(1, this->error);
  return size;
}
#endif

}  // namespace esphome::api
