// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#include "api_pb2.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"
#include <cstring>

namespace esphome::api {

bool HelloRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 2):
      PROTO_DECODE_GUARD(tag, 1, 2);
      this->client_info = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->api_version_major = value.as_varint();
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->api_version_minor = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *HelloResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const HelloResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.api_version_major);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.api_version_minor);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.server_info);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 34, msg.name);
  return pos;
}
uint32_t HelloResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const HelloResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.api_version_major);
  size += ProtoSize::calc_uint32(1, msg.api_version_minor);
  size += 2 + msg.server_info.size();
  size += 2 + msg.name.size();
  return size;
}
bool DisconnectRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->reason = static_cast<enums::DisconnectReason>(value.as_varint());
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *DisconnectRequest::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const DisconnectRequest *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, static_cast<uint32_t>(msg.reason));
  return pos;
}
uint32_t DisconnectRequest::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const DisconnectRequest *>(self);
  uint32_t size = 0;
  size += msg.reason ? 2 : 0;
  return size;
}
#ifdef USE_AREAS
uint8_t *AreaInfo::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const AreaInfo *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.area_id);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 18, msg.name);
  return pos;
}
uint32_t AreaInfo::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const AreaInfo *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.area_id);
  size += 2 + msg.name.size();
  return size;
}
#endif
#ifdef USE_DEVICES
uint8_t *DeviceInfo::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const DeviceInfo *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.device_id);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 18, msg.name);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.area_id);
  return pos;
}
uint32_t DeviceInfo::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const DeviceInfo *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.device_id);
  size += 2 + msg.name.size();
  size += ProtoSize::calc_uint32(1, msg.area_id);
  return size;
}
#endif
#ifdef USE_SERIAL_PROXY
uint8_t *SerialProxyInfo::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const SerialProxyInfo *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.name);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(msg.port_type));
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.configured_line_states);
  return pos;
}
uint32_t SerialProxyInfo::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const SerialProxyInfo *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, msg.name.size());
  size += msg.port_type ? 2 : 0;
  size += ProtoSize::calc_uint32(1, msg.configured_line_states);
  return size;
}
#endif
uint8_t *DeviceInfoResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const DeviceInfoResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 18, msg.name);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.mac_address);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 34, msg.esphome_version);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 42, msg.compilation_time);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 50, msg.model);
#ifdef USE_DEEP_SLEEP
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.has_deep_sleep);
#endif
#ifdef ESPHOME_PROJECT_NAME
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 66, msg.project_name);
#endif
#ifdef ESPHOME_PROJECT_NAME
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 74, msg.project_version);
#endif
#ifdef USE_WEBSERVER
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.webserver_port);
#endif
#ifdef USE_BLUETOOTH_PROXY
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 15, msg.bluetooth_proxy_feature_flags);
#endif
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 98, msg.manufacturer);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 106, msg.friendly_name);
#ifdef USE_VOICE_ASSISTANT
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 17, msg.voice_assistant_feature_flags);
#endif
#ifdef USE_AREAS
  pos = ProtoEncode::encode_string_force(pos PROTO_ENCODE_DEBUG_ARG, 16, msg.suggested_area);
#endif
#ifdef USE_BLUETOOTH_PROXY
  pos = ProtoEncode::encode_string_force(pos PROTO_ENCODE_DEBUG_ARG, 18, msg.bluetooth_mac_address);
#endif
#ifdef USE_API_NOISE
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 19, msg.api_encryption_supported);
#endif
#ifdef USE_DEVICES
  for (const auto &it : msg.devices) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 20, it);
  }
#endif
#ifdef USE_AREAS
  for (const auto &it : msg.areas) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 21, it);
  }
#endif
#ifdef USE_AREAS
  pos = ProtoEncode::encode_optional_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 22, msg.area);
#endif
#ifdef USE_ZWAVE_PROXY
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 23, msg.zwave_proxy_feature_flags);
#endif
#ifdef USE_ZWAVE_PROXY
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 24, msg.zwave_home_id);
#endif
#ifdef USE_SERIAL_PROXY
  for (const auto &it : msg.serial_proxies) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 25, it);
  }
#endif
#ifdef USE_API_NOISE
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.api_encryption_provisionable);
#endif
  return pos;
}
uint32_t DeviceInfoResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const DeviceInfoResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.name.size();
  size += 2 + msg.mac_address.size();
  size += 2 + msg.esphome_version.size();
  size += 2 + msg.compilation_time.size();
  size += 2 + msg.model.size();
#ifdef USE_DEEP_SLEEP
  size += ProtoSize::calc_bool(1, msg.has_deep_sleep);
#endif
#ifdef ESPHOME_PROJECT_NAME
  size += 2 + msg.project_name.size();
#endif
#ifdef ESPHOME_PROJECT_NAME
  size += 2 + msg.project_version.size();
#endif
#ifdef USE_WEBSERVER
  size += ProtoSize::calc_uint32(1, msg.webserver_port);
#endif
#ifdef USE_BLUETOOTH_PROXY
  size += ProtoSize::calc_uint32(1, msg.bluetooth_proxy_feature_flags);
#endif
  size += 2 + msg.manufacturer.size();
  size += 2 + msg.friendly_name.size();
#ifdef USE_VOICE_ASSISTANT
  size += ProtoSize::calc_uint32(2, msg.voice_assistant_feature_flags);
#endif
#ifdef USE_AREAS
  size += 3 + msg.suggested_area.size();
#endif
#ifdef USE_BLUETOOTH_PROXY
  size += 3 + msg.bluetooth_mac_address.size();
#endif
#ifdef USE_API_NOISE
  size += ProtoSize::calc_bool(2, msg.api_encryption_supported);
#endif
#ifdef USE_DEVICES
  for (const auto &it : msg.devices) {
    size += ProtoSize::calc_message_force(2, it.calculate_size());
  }
#endif
#ifdef USE_AREAS
  for (const auto &it : msg.areas) {
    size += ProtoSize::calc_message_force(2, it.calculate_size());
  }
#endif
#ifdef USE_AREAS
  size += ProtoSize::calc_message(2, msg.area.calculate_size());
#endif
#ifdef USE_ZWAVE_PROXY
  size += ProtoSize::calc_uint32(2, msg.zwave_proxy_feature_flags);
#endif
#ifdef USE_ZWAVE_PROXY
  size += ProtoSize::calc_uint32(2, msg.zwave_home_id);
#endif
#ifdef USE_SERIAL_PROXY
  for (const auto &it : msg.serial_proxies) {
    size += ProtoSize::calc_message_force(2, it.calculate_size());
  }
#endif
#ifdef USE_API_NOISE
  size += ProtoSize::calc_bool(2, msg.api_encryption_provisionable);
#endif
  return size;
}
#ifdef USE_BLUETOOTH_PROXY
uint8_t *BluetoothProxyCapabilities::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothProxyCapabilities *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.feature_flags);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 18, msg.mac_address);
  return pos;
}
uint32_t BluetoothProxyCapabilities::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothProxyCapabilities *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.feature_flags);
  size += 2 + msg.mac_address.size();
  return size;
}
#endif
#ifdef USE_VOICE_ASSISTANT
uint8_t *VoiceAssistantCapabilities::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const VoiceAssistantCapabilities *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.feature_flags);
  return pos;
}
uint32_t VoiceAssistantCapabilities::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const VoiceAssistantCapabilities *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.feature_flags);
  return size;
}
#endif
#ifdef USE_ZWAVE_PROXY
uint8_t *ZWaveProxyCapabilities::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ZWaveProxyCapabilities *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.feature_flags);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.home_id);
  return pos;
}
uint32_t ZWaveProxyCapabilities::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ZWaveProxyCapabilities *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.feature_flags);
  size += ProtoSize::calc_uint32(1, msg.home_id);
  return size;
}
#endif
uint8_t *DeviceCapabilitiesResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const DeviceCapabilitiesResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
#ifdef USE_BLUETOOTH_PROXY
  pos = ProtoEncode::encode_optional_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 1, msg.bluetooth_proxy);
#endif
#ifdef USE_VOICE_ASSISTANT
  pos = ProtoEncode::encode_optional_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 2, msg.voice_assistant);
#endif
#ifdef USE_ZWAVE_PROXY
  pos = ProtoEncode::encode_optional_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 3, msg.zwave_proxy);
#endif
#ifdef USE_SERIAL_PROXY
  for (const auto &it : msg.serial_proxies) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 4, it);
  }
#endif
  return pos;
}
uint32_t DeviceCapabilitiesResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const DeviceCapabilitiesResponse *>(self);
  uint32_t size = 0;
#ifdef USE_BLUETOOTH_PROXY
  size += ProtoSize::calc_message(1, msg.bluetooth_proxy.calculate_size());
#endif
#ifdef USE_VOICE_ASSISTANT
  size += ProtoSize::calc_message(1, msg.voice_assistant.calculate_size());
#endif
#ifdef USE_ZWAVE_PROXY
  size += ProtoSize::calc_message(1, msg.zwave_proxy.calculate_size());
#endif
#ifdef USE_SERIAL_PROXY
  for (const auto &it : msg.serial_proxies) {
    size += ProtoSize::calc_message_force(1, it.calculate_size());
  }
#endif
  return size;
}
#ifdef USE_BINARY_SENSOR
uint8_t *ListEntitiesBinarySensorResponse::encode_msg(const void *self,
                                                      ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesBinarySensorResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.device_class);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.is_status_binary_sensor);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.icon);
#endif
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesBinarySensorResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesBinarySensorResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
  size += !msg.device_class.empty() ? 2 + msg.device_class.size() : 0;
  size += ProtoSize::calc_bool(1, msg.is_status_binary_sensor);
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *BinarySensorStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BinarySensorStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.state);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.missing_state);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.device_id);
#endif
  return pos;
}
uint32_t BinarySensorStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BinarySensorStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, msg.state);
  size += ProtoSize::calc_bool(1, msg.missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
#endif
#ifdef USE_COVER
uint8_t *ListEntitiesCoverResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesCoverResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.assumed_state);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.supports_position);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.supports_tilt);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.device_class);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.icon);
#endif
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 12, msg.supports_stop);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesCoverResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesCoverResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
  size += ProtoSize::calc_bool(1, msg.assumed_state);
  size += ProtoSize::calc_bool(1, msg.supports_position);
  size += ProtoSize::calc_bool(1, msg.supports_tilt);
  size += !msg.device_class.empty() ? 2 + msg.device_class.size() : 0;
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += msg.entity_category ? 2 : 0;
  size += ProtoSize::calc_bool(1, msg.supports_stop);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *CoverStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const CoverStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  if (uint32_t raw = float_to_raw(msg.position); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 29, raw);
  }
  if (uint32_t raw = float_to_raw(msg.tilt); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 37, raw);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, static_cast<uint32_t>(msg.current_operation));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.device_id);
#endif
  return pos;
}
uint32_t CoverStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const CoverStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, msg.position);
  size += ProtoSize::calc_float(1, msg.tilt);
  size += msg.current_operation ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool CoverCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->has_position = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(5, 5):
      PROTO_DECODE_GUARD(tag, 5, 5);
      this->position = value.as_float();
      break;
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->has_tilt = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(7, 5):
      PROTO_DECODE_GUARD(tag, 7, 5);
      this->tilt = value.as_float();
      break;
    case PROTO_DECODE_CASE(8, 0):
      PROTO_DECODE_GUARD(tag, 8, 0);
      this->stop = value.as_varint() != 0;
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(9, 0):
      PROTO_DECODE_GUARD(tag, 9, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_FAN
uint8_t *ListEntitiesFanResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesFanResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.supports_oscillation);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.supports_speed);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.supports_direction);
  pos = ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.supported_speed_count);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.icon);
#endif
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(msg.entity_category));
  for (const char *it : *msg.supported_preset_modes) {
    pos = ProtoEncode::encode_string_force(pos PROTO_ENCODE_DEBUG_ARG, 12, it, strlen(it));
  }
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesFanResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesFanResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
  size += ProtoSize::calc_bool(1, msg.supports_oscillation);
  size += ProtoSize::calc_bool(1, msg.supports_speed);
  size += ProtoSize::calc_bool(1, msg.supports_direction);
  size += ProtoSize::calc_int32(1, msg.supported_speed_count);
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += msg.entity_category ? 2 : 0;
  if (!msg.supported_preset_modes->empty()) {
    for (const char *it : *msg.supported_preset_modes) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *FanStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const FanStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.state);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.oscillating);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, static_cast<uint32_t>(msg.direction));
  pos = ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.speed_level);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.preset_mode);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.device_id);
#endif
  return pos;
}
uint32_t FanStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const FanStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, msg.state);
  size += ProtoSize::calc_bool(1, msg.oscillating);
  size += msg.direction ? 2 : 0;
  size += ProtoSize::calc_int32(1, msg.speed_level);
  size += ProtoSize::calc_length(1, msg.preset_mode.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool FanCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->has_state = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->state = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->has_oscillating = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(7, 0):
      PROTO_DECODE_GUARD(tag, 7, 0);
      this->oscillating = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(8, 0):
      PROTO_DECODE_GUARD(tag, 8, 0);
      this->has_direction = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(9, 0):
      PROTO_DECODE_GUARD(tag, 9, 0);
      this->direction = static_cast<enums::FanDirection>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(10, 0):
      PROTO_DECODE_GUARD(tag, 10, 0);
      this->has_speed_level = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(11, 0):
      PROTO_DECODE_GUARD(tag, 11, 0);
      this->speed_level = static_cast<int32_t>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(12, 0):
      PROTO_DECODE_GUARD(tag, 12, 0);
      this->has_preset_mode = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(13, 2):
      PROTO_DECODE_GUARD(tag, 13, 2);
      this->preset_mode = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(14, 0):
      PROTO_DECODE_GUARD(tag, 14, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_LIGHT
uint8_t *ListEntitiesLightResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesLightResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
  for (const auto &it : *msg.supported_color_modes) {
    pos = ProtoEncode::encode_uint32_force(pos PROTO_ENCODE_DEBUG_ARG, 12, static_cast<uint32_t>(it));
  }
  if (uint32_t raw = float_to_raw(msg.min_mireds); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 77, raw);
  }
  if (uint32_t raw = float_to_raw(msg.max_mireds); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 85, raw);
  }
  for (const char *it : *msg.effects) {
    pos = ProtoEncode::encode_string_force(pos PROTO_ENCODE_DEBUG_ARG, 11, it, strlen(it));
  }
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 14, msg.icon);
#endif
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 15, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 16, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesLightResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesLightResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
  if (!msg.supported_color_modes->empty()) {
    size += msg.supported_color_modes->size() * 2;
  }
  size += ProtoSize::calc_float(1, msg.min_mireds);
  size += ProtoSize::calc_float(1, msg.max_mireds);
  if (!msg.effects->empty()) {
    for (const char *it : *msg.effects) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(2, msg.device_id);
#endif
  return size;
}
uint8_t *LightStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const LightStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.state);
  if (uint32_t raw = float_to_raw(msg.brightness); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 29, raw);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(msg.color_mode));
  if (uint32_t raw = float_to_raw(msg.color_brightness); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 85, raw);
  }
  if (uint32_t raw = float_to_raw(msg.red); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 37, raw);
  }
  if (uint32_t raw = float_to_raw(msg.green); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 45, raw);
  }
  if (uint32_t raw = float_to_raw(msg.blue); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 53, raw);
  }
  if (uint32_t raw = float_to_raw(msg.white); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 61, raw);
  }
  if (uint32_t raw = float_to_raw(msg.color_temperature); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 69, raw);
  }
  if (uint32_t raw = float_to_raw(msg.cold_white); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 101, raw);
  }
  if (uint32_t raw = float_to_raw(msg.warm_white); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 109, raw);
  }
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.effect);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 14, msg.device_id);
#endif
  return pos;
}
uint32_t LightStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const LightStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, msg.state);
  size += ProtoSize::calc_float(1, msg.brightness);
  size += msg.color_mode ? 2 : 0;
  size += ProtoSize::calc_float(1, msg.color_brightness);
  size += ProtoSize::calc_float(1, msg.red);
  size += ProtoSize::calc_float(1, msg.green);
  size += ProtoSize::calc_float(1, msg.blue);
  size += ProtoSize::calc_float(1, msg.white);
  size += ProtoSize::calc_float(1, msg.color_temperature);
  size += ProtoSize::calc_float(1, msg.cold_white);
  size += ProtoSize::calc_float(1, msg.warm_white);
  size += ProtoSize::calc_length(1, msg.effect.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool LightCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->has_state = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->state = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->has_brightness = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(5, 5):
      PROTO_DECODE_GUARD(tag, 5, 5);
      this->brightness = value.as_float();
      break;
    case PROTO_DECODE_CASE(22, 0):
      PROTO_DECODE_GUARD(tag, 22, 0);
      this->has_color_mode = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(23, 0):
      PROTO_DECODE_GUARD(tag, 23, 0);
      this->color_mode = static_cast<enums::ColorMode>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(20, 0):
      PROTO_DECODE_GUARD(tag, 20, 0);
      this->has_color_brightness = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(21, 5):
      PROTO_DECODE_GUARD(tag, 21, 5);
      this->color_brightness = value.as_float();
      break;
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->has_rgb = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(7, 5):
      PROTO_DECODE_GUARD(tag, 7, 5);
      this->red = value.as_float();
      break;
    case PROTO_DECODE_CASE(8, 5):
      PROTO_DECODE_GUARD(tag, 8, 5);
      this->green = value.as_float();
      break;
    case PROTO_DECODE_CASE(9, 5):
      PROTO_DECODE_GUARD(tag, 9, 5);
      this->blue = value.as_float();
      break;
    case PROTO_DECODE_CASE(10, 0):
      PROTO_DECODE_GUARD(tag, 10, 0);
      this->has_white = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(11, 5):
      PROTO_DECODE_GUARD(tag, 11, 5);
      this->white = value.as_float();
      break;
    case PROTO_DECODE_CASE(12, 0):
      PROTO_DECODE_GUARD(tag, 12, 0);
      this->has_color_temperature = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(13, 5):
      PROTO_DECODE_GUARD(tag, 13, 5);
      this->color_temperature = value.as_float();
      break;
    case PROTO_DECODE_CASE(24, 0):
      PROTO_DECODE_GUARD(tag, 24, 0);
      this->has_cold_white = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(25, 5):
      PROTO_DECODE_GUARD(tag, 25, 5);
      this->cold_white = value.as_float();
      break;
    case PROTO_DECODE_CASE(26, 0):
      PROTO_DECODE_GUARD(tag, 26, 0);
      this->has_warm_white = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(27, 5):
      PROTO_DECODE_GUARD(tag, 27, 5);
      this->warm_white = value.as_float();
      break;
    case PROTO_DECODE_CASE(14, 0):
      PROTO_DECODE_GUARD(tag, 14, 0);
      this->has_transition_length = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(15, 0):
      PROTO_DECODE_GUARD(tag, 15, 0);
      this->transition_length = value.as_varint();
      break;
    case PROTO_DECODE_CASE(16, 0):
      PROTO_DECODE_GUARD(tag, 16, 0);
      this->has_flash_length = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(17, 0):
      PROTO_DECODE_GUARD(tag, 17, 0);
      this->flash_length = value.as_varint();
      break;
    case PROTO_DECODE_CASE(18, 0):
      PROTO_DECODE_GUARD(tag, 18, 0);
      this->has_effect = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(19, 2):
      PROTO_DECODE_GUARD(tag, 19, 2);
      this->effect = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(28, 0):
      PROTO_DECODE_GUARD(tag, 28, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_SENSOR
uint8_t *ListEntitiesSensorResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesSensorResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.unit_of_measurement);
  pos = ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.accuracy_decimals);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.force_update);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.device_class);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(msg.state_class));
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 12, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 14, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesSensorResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesSensorResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += !msg.unit_of_measurement.empty() ? 2 + msg.unit_of_measurement.size() : 0;
  size += ProtoSize::calc_int32(1, msg.accuracy_decimals);
  size += ProtoSize::calc_bool(1, msg.force_update);
  size += !msg.device_class.empty() ? 2 + msg.device_class.size() : 0;
  size += msg.state_class ? 2 : 0;
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
SensorStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const SensorStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  if (uint32_t raw = float_to_raw(msg.state); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, raw);
  }
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.missing_state);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.device_id);
#endif
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
SensorStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const SensorStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, msg.state);
  size += ProtoSize::calc_bool(1, msg.missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
#endif
#ifdef USE_SWITCH
uint8_t *ListEntitiesSwitchResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesSwitchResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.assumed_state);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.device_class);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesSwitchResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesSwitchResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.assumed_state);
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
  size += !msg.device_class.empty() ? 2 + msg.device_class.size() : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *SwitchStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const SwitchStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.state);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.device_id);
#endif
  return pos;
}
uint32_t SwitchStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const SwitchStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, msg.state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool SwitchCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->state = value.as_varint() != 0;
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_TEXT_SENSOR
uint8_t *ListEntitiesTextSensorResponse::encode_msg(const void *self,
                                                    ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesTextSensorResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.device_class);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesTextSensorResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesTextSensorResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
  size += !msg.device_class.empty() ? 2 + msg.device_class.size() : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *TextSensorStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const TextSensorStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.state);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.missing_state);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.device_id);
#endif
  return pos;
}
uint32_t TextSensorStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const TextSensorStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_length(1, msg.state.size());
  size += ProtoSize::calc_bool(1, msg.missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
#endif
bool SubscribeLogsRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->level = static_cast<enums::LogLevel>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->dump_config = value.as_varint() != 0;
      break;
    default:
      return false;
  }
  return true;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
SubscribeLogsResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const SubscribeLogsResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32_force(pos PROTO_ENCODE_DEBUG_ARG, 1, static_cast<uint32_t>(msg.level));
  pos = ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 26);
  pos = ProtoEncode::encode_varint_raw(pos PROTO_ENCODE_DEBUG_ARG, msg.message_len_);
  pos = ProtoEncode::encode_raw(pos PROTO_ENCODE_DEBUG_ARG, msg.message_ptr_, msg.message_len_);
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
SubscribeLogsResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const SubscribeLogsResponse *>(self);
  uint32_t size = 0;
  size += 2;
  size += ProtoSize::calc_length_force(1, msg.message_len_);
  return size;
}
#ifdef USE_API_NOISE
bool NoiseEncryptionSetKeyRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 2):
      PROTO_DECODE_GUARD(tag, 1, 2);
      this->key = value.data();
      this->key_len = value.size();
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *NoiseEncryptionSetKeyResponse::encode_msg(const void *self,
                                                   ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const NoiseEncryptionSetKeyResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.success);
  return pos;
}
uint32_t NoiseEncryptionSetKeyResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const NoiseEncryptionSetKeyResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_bool(1, msg.success);
  return size;
}
#endif
#ifdef USE_API_HOMEASSISTANT_SERVICES
uint8_t *HomeassistantServiceMap::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const HomeassistantServiceMap *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.key);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.value);
  return pos;
}
uint32_t HomeassistantServiceMap::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const HomeassistantServiceMap *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, msg.key.size());
  size += ProtoSize::calc_length(1, msg.value.size());
  return size;
}
uint8_t *HomeassistantActionRequest::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const HomeassistantActionRequest *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.service);
  for (auto &it : msg.data) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 2, it);
  }
  for (auto &it : msg.data_template) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 3, it);
  }
  for (auto &it : msg.variables) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 4, it);
  }
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.is_event);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.call_id);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.wants_response);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.response_template);
#endif
  return pos;
}
uint32_t HomeassistantActionRequest::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const HomeassistantActionRequest *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, msg.service.size());
  if (!msg.data.empty()) {
    for (const auto &it : msg.data) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  if (!msg.data_template.empty()) {
    for (const auto &it : msg.data_template) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  if (!msg.variables.empty()) {
    for (const auto &it : msg.variables) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  size += ProtoSize::calc_bool(1, msg.is_event);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  size += ProtoSize::calc_uint32(1, msg.call_id);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  size += ProtoSize::calc_bool(1, msg.wants_response);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  size += ProtoSize::calc_length(1, msg.response_template.size());
#endif
  return size;
}
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
bool HomeassistantActionResponse::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->call_id = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->success = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(3, 2):
      PROTO_DECODE_GUARD(tag, 3, 2);
      this->error_message = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
    case PROTO_DECODE_CASE(4, 2):
      PROTO_DECODE_GUARD(tag, 4, 2);
      this->response_data = value.data();
      this->response_data_len = value.size();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
uint8_t *SubscribeHomeAssistantStateResponse::encode_msg(const void *self,
                                                         ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const SubscribeHomeAssistantStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.entity_id);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.attribute);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.once);
  return pos;
}
uint32_t SubscribeHomeAssistantStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const SubscribeHomeAssistantStateResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, msg.entity_id.size());
  size += ProtoSize::calc_length(1, msg.attribute.size());
  size += ProtoSize::calc_bool(1, msg.once);
  return size;
}
bool HomeAssistantStateResponse::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 2):
      PROTO_DECODE_GUARD(tag, 1, 2);
      this->entity_id = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(2, 2):
      PROTO_DECODE_GUARD(tag, 2, 2);
      this->state = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(3, 2):
      PROTO_DECODE_GUARD(tag, 3, 2);
      this->attribute = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    default:
      return false;
  }
  return true;
}
#endif
bool DSTRule::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->time_seconds = decode_zigzag32(static_cast<uint32_t>(value.as_varint()));
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->day = value.as_varint();
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->type = static_cast<enums::DSTRuleType>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->month = value.as_varint();
      break;
    case PROTO_DECODE_CASE(5, 0):
      PROTO_DECODE_GUARD(tag, 5, 0);
      this->week = value.as_varint();
      break;
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->day_of_week = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
bool ParsedTimezone::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->std_offset_seconds = decode_zigzag32(static_cast<uint32_t>(value.as_varint()));
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->dst_offset_seconds = decode_zigzag32(static_cast<uint32_t>(value.as_varint()));
      break;
    case PROTO_DECODE_CASE(3, 2):
      PROTO_DECODE_GUARD(tag, 3, 2);
      value.decode_to_message(this->dst_start);
      break;
    case PROTO_DECODE_CASE(4, 2):
      PROTO_DECODE_GUARD(tag, 4, 2);
      value.decode_to_message(this->dst_end);
      break;
    default:
      return false;
  }
  return true;
}
bool GetTimeResponse::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->epoch_seconds = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(3, 2):
      PROTO_DECODE_GUARD(tag, 3, 2);
      value.decode_to_message(this->parsed_timezone);
      this->has_parsed_timezone = true;
      break;
    default:
      return false;
  }
  return true;
}
#ifdef USE_API_USER_DEFINED_ACTIONS
uint8_t *ListEntitiesServicesArgument::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesServicesArgument *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.name);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(msg.type));
#ifdef USE_API_USER_DEFINED_ACTION_METADATA
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.description);
#endif
#ifdef USE_API_USER_DEFINED_ACTION_METADATA
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.example);
#endif
  return pos;
}
uint32_t ListEntitiesServicesArgument::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesServicesArgument *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, msg.name.size());
  size += msg.type ? 2 : 0;
#ifdef USE_API_USER_DEFINED_ACTION_METADATA
  size += ProtoSize::calc_length(1, msg.description.size());
#endif
#ifdef USE_API_USER_DEFINED_ACTION_METADATA
  size += ProtoSize::calc_length(1, msg.example.size());
#endif
  return size;
}
uint8_t *ListEntitiesServicesResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesServicesResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.name);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  for (auto &it : msg.args) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 3, it);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, static_cast<uint32_t>(msg.supports_response));
#ifdef USE_API_USER_DEFINED_ACTION_METADATA
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.description);
#endif
  return pos;
}
uint32_t ListEntitiesServicesResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesServicesResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, msg.name.size());
  size += 5;
  if (!msg.args.empty()) {
    for (const auto &it : msg.args) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  size += msg.supports_response ? 2 : 0;
#ifdef USE_API_USER_DEFINED_ACTION_METADATA
  size += ProtoSize::calc_length(1, msg.description.size());
#endif
  return size;
}
bool ExecuteServiceArgument::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->bool_ = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->legacy_int = static_cast<int32_t>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(3, 5):
      PROTO_DECODE_GUARD(tag, 3, 5);
      this->float_ = value.as_float();
      break;
    case PROTO_DECODE_CASE(4, 2):
      PROTO_DECODE_GUARD(tag, 4, 2);
      this->string_ = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(5, 0):
      PROTO_DECODE_GUARD(tag, 5, 0);
      this->int_ = decode_zigzag32(static_cast<uint32_t>(value.as_varint()));
      break;
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->bool_array.push_back(value.as_varint() != 0);
      break;
    case PROTO_DECODE_CASE(7, 0):
      PROTO_DECODE_GUARD(tag, 7, 0);
      this->int_array.push_back(decode_zigzag32(static_cast<uint32_t>(value.as_varint())));
      break;
    case PROTO_DECODE_CASE(8, 5):
      PROTO_DECODE_GUARD(tag, 8, 5);
      this->float_array.push_back(value.as_float());
      break;
    case PROTO_DECODE_CASE(9, 2):
      PROTO_DECODE_GUARD(tag, 9, 2);
      this->string_array.push_back(value.as_string());
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
bool ExecuteServiceRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 2):
      PROTO_DECODE_GUARD(tag, 2, 2);
      this->args.emplace_back();
      value.decode_to_message(this->args.back());
      break;
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->call_id = value.as_varint();
      break;
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->return_response = value.as_varint() != 0;
      break;
#endif
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
uint8_t *ExecuteServiceResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ExecuteServiceResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.call_id);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.success);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.error_message);
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  pos = ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.response_data, msg.response_data_len);
#endif
  return pos;
}
uint32_t ExecuteServiceResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ExecuteServiceResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.call_id);
  size += ProtoSize::calc_bool(1, msg.success);
  size += ProtoSize::calc_length(1, msg.error_message.size());
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  size += ProtoSize::calc_length(1, msg.response_data_len);
#endif
  return size;
}
#endif
#ifdef USE_CAMERA
uint8_t *ListEntitiesCameraResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesCameraResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.icon);
#endif
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesCameraResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesCameraResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *CameraImageResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const CameraImageResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.data_ptr_, msg.data_len_);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.done);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.device_id);
#endif
  return pos;
}
uint32_t CameraImageResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const CameraImageResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_length(1, msg.data_len_);
  size += ProtoSize::calc_bool(1, msg.done);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool CameraImageRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->single = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->stream = value.as_varint() != 0;
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_CLIMATE
uint8_t *ListEntitiesClimateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesClimateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.supports_current_temperature);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.supports_two_point_target_temperature);
  for (const auto &it : *msg.supported_modes) {
    pos = ProtoEncode::encode_uint32_force(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(it));
  }
  if (uint32_t raw = float_to_raw(msg.visual_min_temperature); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 69, raw);
  }
  if (uint32_t raw = float_to_raw(msg.visual_max_temperature); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 77, raw);
  }
  if (uint32_t raw = float_to_raw(msg.visual_target_temperature_step); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 85, raw);
  }
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 12, msg.supports_action);
  for (const auto &it : *msg.supported_fan_modes) {
    pos = ProtoEncode::encode_uint32_force(pos PROTO_ENCODE_DEBUG_ARG, 13, static_cast<uint32_t>(it));
  }
  for (const auto &it : *msg.supported_swing_modes) {
    pos = ProtoEncode::encode_uint32_force(pos PROTO_ENCODE_DEBUG_ARG, 14, static_cast<uint32_t>(it));
  }
  for (const char *it : *msg.supported_custom_fan_modes) {
    pos = ProtoEncode::encode_string_force(pos PROTO_ENCODE_DEBUG_ARG, 15, it, strlen(it));
  }
  for (const auto &it : *msg.supported_presets) {
    pos = ProtoEncode::encode_uint32_force(pos PROTO_ENCODE_DEBUG_ARG, 16, static_cast<uint32_t>(it));
  }
  for (const char *it : *msg.supported_custom_presets) {
    pos = ProtoEncode::encode_string_force(pos PROTO_ENCODE_DEBUG_ARG, 17, it, strlen(it));
  }
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 18, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 19, msg.icon);
#endif
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 20, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.visual_current_temperature_step);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 22, msg.supports_current_humidity);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 23, msg.supports_target_humidity);
  pos = ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 24, msg.visual_min_humidity);
  pos = ProtoEncode::encode_float(pos PROTO_ENCODE_DEBUG_ARG, 25, msg.visual_max_humidity);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.device_id);
#endif
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 27, msg.feature_flags);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 28, static_cast<uint32_t>(msg.temperature_unit));
  return pos;
}
uint32_t ListEntitiesClimateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesClimateResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
  size += ProtoSize::calc_bool(1, msg.supports_current_temperature);
  size += ProtoSize::calc_bool(1, msg.supports_two_point_target_temperature);
  if (!msg.supported_modes->empty()) {
    size += msg.supported_modes->size() * 2;
  }
  size += ProtoSize::calc_float(1, msg.visual_min_temperature);
  size += ProtoSize::calc_float(1, msg.visual_max_temperature);
  size += ProtoSize::calc_float(1, msg.visual_target_temperature_step);
  size += ProtoSize::calc_bool(1, msg.supports_action);
  if (!msg.supported_fan_modes->empty()) {
    size += msg.supported_fan_modes->size() * 2;
  }
  if (!msg.supported_swing_modes->empty()) {
    size += msg.supported_swing_modes->size() * 2;
  }
  if (!msg.supported_custom_fan_modes->empty()) {
    for (const char *it : *msg.supported_custom_fan_modes) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
  if (!msg.supported_presets->empty()) {
    size += msg.supported_presets->size() * 3;
  }
  if (!msg.supported_custom_presets->empty()) {
    for (const char *it : *msg.supported_custom_presets) {
      size += ProtoSize::calc_length_force(2, strlen(it));
    }
  }
  size += ProtoSize::calc_bool(2, msg.disabled_by_default);
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 3 + msg.icon.size() : 0;
#endif
  size += msg.entity_category ? 3 : 0;
  size += ProtoSize::calc_float(2, msg.visual_current_temperature_step);
  size += ProtoSize::calc_bool(2, msg.supports_current_humidity);
  size += ProtoSize::calc_bool(2, msg.supports_target_humidity);
  size += ProtoSize::calc_float(2, msg.visual_min_humidity);
  size += ProtoSize::calc_float(2, msg.visual_max_humidity);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(2, msg.device_id);
#endif
  size += ProtoSize::calc_uint32(2, msg.feature_flags);
  size += msg.temperature_unit ? 3 : 0;
  return size;
}
uint8_t *ClimateStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ClimateStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(msg.mode));
  if (uint32_t raw = float_to_raw(msg.current_temperature); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 29, raw);
  }
  if (uint32_t raw = float_to_raw(msg.target_temperature); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 37, raw);
  }
  if (uint32_t raw = float_to_raw(msg.target_temperature_low); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 45, raw);
  }
  if (uint32_t raw = float_to_raw(msg.target_temperature_high); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 53, raw);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, static_cast<uint32_t>(msg.action));
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, static_cast<uint32_t>(msg.fan_mode));
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(msg.swing_mode));
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 11, msg.custom_fan_mode);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, static_cast<uint32_t>(msg.preset));
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.custom_preset);
  if (uint32_t raw = float_to_raw(msg.current_humidity); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 117, raw);
  }
  if (uint32_t raw = float_to_raw(msg.target_humidity); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 125, raw);
  }
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 16, msg.device_id);
#endif
  return pos;
}
uint32_t ClimateStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ClimateStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += msg.mode ? 2 : 0;
  size += ProtoSize::calc_float(1, msg.current_temperature);
  size += ProtoSize::calc_float(1, msg.target_temperature);
  size += ProtoSize::calc_float(1, msg.target_temperature_low);
  size += ProtoSize::calc_float(1, msg.target_temperature_high);
  size += msg.action ? 2 : 0;
  size += msg.fan_mode ? 2 : 0;
  size += msg.swing_mode ? 2 : 0;
  size += ProtoSize::calc_length(1, msg.custom_fan_mode.size());
  size += msg.preset ? 2 : 0;
  size += ProtoSize::calc_length(1, msg.custom_preset.size());
  size += ProtoSize::calc_float(1, msg.current_humidity);
  size += ProtoSize::calc_float(1, msg.target_humidity);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(2, msg.device_id);
#endif
  return size;
}
bool ClimateCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->has_mode = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->mode = static_cast<enums::ClimateMode>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->has_target_temperature = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(5, 5):
      PROTO_DECODE_GUARD(tag, 5, 5);
      this->target_temperature = value.as_float();
      break;
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->has_target_temperature_low = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(7, 5):
      PROTO_DECODE_GUARD(tag, 7, 5);
      this->target_temperature_low = value.as_float();
      break;
    case PROTO_DECODE_CASE(8, 0):
      PROTO_DECODE_GUARD(tag, 8, 0);
      this->has_target_temperature_high = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(9, 5):
      PROTO_DECODE_GUARD(tag, 9, 5);
      this->target_temperature_high = value.as_float();
      break;
    case PROTO_DECODE_CASE(12, 0):
      PROTO_DECODE_GUARD(tag, 12, 0);
      this->has_fan_mode = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(13, 0):
      PROTO_DECODE_GUARD(tag, 13, 0);
      this->fan_mode = static_cast<enums::ClimateFanMode>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(14, 0):
      PROTO_DECODE_GUARD(tag, 14, 0);
      this->has_swing_mode = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(15, 0):
      PROTO_DECODE_GUARD(tag, 15, 0);
      this->swing_mode = static_cast<enums::ClimateSwingMode>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(16, 0):
      PROTO_DECODE_GUARD(tag, 16, 0);
      this->has_custom_fan_mode = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(17, 2):
      PROTO_DECODE_GUARD(tag, 17, 2);
      this->custom_fan_mode = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(18, 0):
      PROTO_DECODE_GUARD(tag, 18, 0);
      this->has_preset = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(19, 0):
      PROTO_DECODE_GUARD(tag, 19, 0);
      this->preset = static_cast<enums::ClimatePreset>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(20, 0):
      PROTO_DECODE_GUARD(tag, 20, 0);
      this->has_custom_preset = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(21, 2):
      PROTO_DECODE_GUARD(tag, 21, 2);
      this->custom_preset = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(22, 0):
      PROTO_DECODE_GUARD(tag, 22, 0);
      this->has_target_humidity = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(23, 5):
      PROTO_DECODE_GUARD(tag, 23, 5);
      this->target_humidity = value.as_float();
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(24, 0):
      PROTO_DECODE_GUARD(tag, 24, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_WATER_HEATER
uint8_t *ListEntitiesWaterHeaterResponse::encode_msg(const void *self,
                                                     ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesWaterHeaterResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.device_id);
#endif
  if (uint32_t raw = float_to_raw(msg.min_temperature); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 69, raw);
  }
  if (uint32_t raw = float_to_raw(msg.max_temperature); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 77, raw);
  }
  if (uint32_t raw = float_to_raw(msg.target_temperature_step); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 85, raw);
  }
  for (const auto &it : *msg.supported_modes) {
    pos = ProtoEncode::encode_uint32_force(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(it));
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, msg.supported_features);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 13, static_cast<uint32_t>(msg.temperature_unit));
  return pos;
}
uint32_t ListEntitiesWaterHeaterResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesWaterHeaterResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  size += ProtoSize::calc_float(1, msg.min_temperature);
  size += ProtoSize::calc_float(1, msg.max_temperature);
  size += ProtoSize::calc_float(1, msg.target_temperature_step);
  if (!msg.supported_modes->empty()) {
    size += msg.supported_modes->size() * 2;
  }
  size += ProtoSize::calc_uint32(1, msg.supported_features);
  size += msg.temperature_unit ? 2 : 0;
  return size;
}
uint8_t *WaterHeaterStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const WaterHeaterStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  if (uint32_t raw = float_to_raw(msg.current_temperature); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, raw);
  }
  if (uint32_t raw = float_to_raw(msg.target_temperature); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 29, raw);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, static_cast<uint32_t>(msg.mode));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.device_id);
#endif
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.state);
  if (uint32_t raw = float_to_raw(msg.target_temperature_low); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 61, raw);
  }
  if (uint32_t raw = float_to_raw(msg.target_temperature_high); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 69, raw);
  }
  return pos;
}
uint32_t WaterHeaterStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const WaterHeaterStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, msg.current_temperature);
  size += ProtoSize::calc_float(1, msg.target_temperature);
  size += msg.mode ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  size += ProtoSize::calc_uint32(1, msg.state);
  size += ProtoSize::calc_float(1, msg.target_temperature_low);
  size += ProtoSize::calc_float(1, msg.target_temperature_high);
  return size;
}
bool WaterHeaterCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->has_fields = value.as_varint();
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->mode = static_cast<enums::WaterHeaterMode>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(4, 5):
      PROTO_DECODE_GUARD(tag, 4, 5);
      this->target_temperature = value.as_float();
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(5, 0):
      PROTO_DECODE_GUARD(tag, 5, 0);
      this->device_id = value.as_varint();
      break;
#endif
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->state = value.as_varint();
      break;
    case PROTO_DECODE_CASE(7, 5):
      PROTO_DECODE_GUARD(tag, 7, 5);
      this->target_temperature_low = value.as_float();
      break;
    case PROTO_DECODE_CASE(8, 5):
      PROTO_DECODE_GUARD(tag, 8, 5);
      this->target_temperature_high = value.as_float();
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_NUMBER
uint8_t *ListEntitiesNumberResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesNumberResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  if (uint32_t raw = float_to_raw(msg.min_value); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 53, raw);
  }
  if (uint32_t raw = float_to_raw(msg.max_value); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 61, raw);
  }
  if (uint32_t raw = float_to_raw(msg.step); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 69, raw);
  }
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 11, msg.unit_of_measurement);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, static_cast<uint32_t>(msg.mode));
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.device_class);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 14, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesNumberResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesNumberResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_float(1, msg.min_value);
  size += ProtoSize::calc_float(1, msg.max_value);
  size += ProtoSize::calc_float(1, msg.step);
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
  size += !msg.unit_of_measurement.empty() ? 2 + msg.unit_of_measurement.size() : 0;
  size += msg.mode ? 2 : 0;
  size += !msg.device_class.empty() ? 2 + msg.device_class.size() : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *NumberStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const NumberStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  if (uint32_t raw = float_to_raw(msg.state); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, raw);
  }
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.missing_state);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.device_id);
#endif
  return pos;
}
uint32_t NumberStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const NumberStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, msg.state);
  size += ProtoSize::calc_bool(1, msg.missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool NumberCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 5):
      PROTO_DECODE_GUARD(tag, 2, 5);
      this->state = value.as_float();
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_SELECT
uint8_t *ListEntitiesSelectResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesSelectResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  for (const char *it : *msg.options) {
    pos = ProtoEncode::encode_string_force(pos PROTO_ENCODE_DEBUG_ARG, 6, it, strlen(it));
  }
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesSelectResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesSelectResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  if (!msg.options->empty()) {
    for (const char *it : *msg.options) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *SelectStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const SelectStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.state);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.missing_state);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.device_id);
#endif
  return pos;
}
uint32_t SelectStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const SelectStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_length(1, msg.state.size());
  size += ProtoSize::calc_bool(1, msg.missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool SelectCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 2):
      PROTO_DECODE_GUARD(tag, 2, 2);
      this->state = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_SIREN
uint8_t *ListEntitiesSirenResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesSirenResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  for (const char *it : *msg.tones) {
    pos = ProtoEncode::encode_string_force(pos PROTO_ENCODE_DEBUG_ARG, 7, it, strlen(it));
  }
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.supports_duration);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.supports_volume);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesSirenResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesSirenResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  if (!msg.tones->empty()) {
    for (const char *it : *msg.tones) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
  size += ProtoSize::calc_bool(1, msg.supports_duration);
  size += ProtoSize::calc_bool(1, msg.supports_volume);
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *SirenStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const SirenStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.state);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.device_id);
#endif
  return pos;
}
uint32_t SirenStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const SirenStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, msg.state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool SirenCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->has_state = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->state = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->has_tone = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(5, 2):
      PROTO_DECODE_GUARD(tag, 5, 2);
      this->tone = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->has_duration = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(7, 0):
      PROTO_DECODE_GUARD(tag, 7, 0);
      this->duration = value.as_varint();
      break;
    case PROTO_DECODE_CASE(8, 0):
      PROTO_DECODE_GUARD(tag, 8, 0);
      this->has_volume = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(9, 5):
      PROTO_DECODE_GUARD(tag, 9, 5);
      this->volume = value.as_float();
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(10, 0):
      PROTO_DECODE_GUARD(tag, 10, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_LOCK
uint8_t *ListEntitiesLockResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesLockResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.assumed_state);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.supports_open);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.requires_code);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 11, msg.code_format);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesLockResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesLockResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
  size += ProtoSize::calc_bool(1, msg.assumed_state);
  size += ProtoSize::calc_bool(1, msg.supports_open);
  size += ProtoSize::calc_bool(1, msg.requires_code);
  size += ProtoSize::calc_length(1, msg.code_format.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *LockStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const LockStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(msg.state));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.device_id);
#endif
  return pos;
}
uint32_t LockStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const LockStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += msg.state ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool LockCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->command = static_cast<enums::LockCommand>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->has_code = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(4, 2):
      PROTO_DECODE_GUARD(tag, 4, 2);
      this->code = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(5, 0):
      PROTO_DECODE_GUARD(tag, 5, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_BUTTON
uint8_t *ListEntitiesButtonResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesButtonResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.device_class);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesButtonResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesButtonResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
  size += !msg.device_class.empty() ? 2 + msg.device_class.size() : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool ButtonCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_MEDIA_PLAYER
uint8_t *MediaPlayerSupportedFormat::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const MediaPlayerSupportedFormat *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.format);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.sample_rate);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.num_channels);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, static_cast<uint32_t>(msg.purpose));
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.sample_bytes);
  return pos;
}
uint32_t MediaPlayerSupportedFormat::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const MediaPlayerSupportedFormat *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, msg.format.size());
  size += ProtoSize::calc_uint32(1, msg.sample_rate);
  size += ProtoSize::calc_uint32(1, msg.num_channels);
  size += msg.purpose ? 2 : 0;
  size += ProtoSize::calc_uint32(1, msg.sample_bytes);
  return size;
}
uint8_t *ListEntitiesMediaPlayerResponse::encode_msg(const void *self,
                                                     ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesMediaPlayerResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
  for (auto &it : msg.supported_formats) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 9, it);
  }
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.device_id);
#endif
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, msg.feature_flags);
  return pos;
}
uint32_t ListEntitiesMediaPlayerResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesMediaPlayerResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
  if (!msg.supported_formats.empty()) {
    for (const auto &it : msg.supported_formats) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  size += ProtoSize::calc_uint32(1, msg.feature_flags);
  return size;
}
uint8_t *MediaPlayerStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const MediaPlayerStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(msg.state));
  if (uint32_t raw = float_to_raw(msg.volume); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 29, raw);
  }
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.muted);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.device_id);
#endif
  return pos;
}
uint32_t MediaPlayerStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const MediaPlayerStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += msg.state ? 2 : 0;
  size += ProtoSize::calc_float(1, msg.volume);
  size += ProtoSize::calc_bool(1, msg.muted);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool MediaPlayerCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->has_command = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->command = static_cast<enums::MediaPlayerCommand>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->has_volume = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(5, 5):
      PROTO_DECODE_GUARD(tag, 5, 5);
      this->volume = value.as_float();
      break;
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->has_media_url = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(7, 2):
      PROTO_DECODE_GUARD(tag, 7, 2);
      this->media_url = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(8, 0):
      PROTO_DECODE_GUARD(tag, 8, 0);
      this->has_announcement = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(9, 0):
      PROTO_DECODE_GUARD(tag, 9, 0);
      this->announcement = value.as_varint() != 0;
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(10, 0):
      PROTO_DECODE_GUARD(tag, 10, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_BLUETOOTH_PROXY
bool SubscribeBluetoothLEAdvertisementsRequest::decode_field(uint32_t tag, const uint8_t *data,
                                                             proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->flags = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
BluetoothLERawAdvertisementsResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothLERawAdvertisementsResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  for (uint16_t i = 0; i < msg.advertisements_len; i++) {
    auto &sub_msg = msg.advertisements[i];
    pos = ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 10);
    uint8_t *len_pos = pos;
    pos = ProtoEncode::reserve_byte(pos PROTO_ENCODE_DEBUG_ARG);
    pos = ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 8);
    pos = ProtoEncode::encode_varint_raw_48bit(pos PROTO_ENCODE_DEBUG_ARG, sub_msg.address);
    pos = ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 16);
    pos = ProtoEncode::encode_varint_raw_short(pos PROTO_ENCODE_DEBUG_ARG, encode_zigzag32(sub_msg.rssi));
    if (sub_msg.address_type) {
      pos = ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 24);
      pos = ProtoEncode::encode_varint_raw(pos PROTO_ENCODE_DEBUG_ARG, sub_msg.address_type);
    }
    pos = ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, 34);
    pos = ProtoEncode::write_raw_byte(pos PROTO_ENCODE_DEBUG_ARG, static_cast<uint8_t>(sub_msg.data_len));
    pos = ProtoEncode::encode_raw(pos PROTO_ENCODE_DEBUG_ARG, sub_msg.data, sub_msg.data_len);
    *len_pos = static_cast<uint8_t>(pos - len_pos - 1);
  }
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
BluetoothLERawAdvertisementsResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothLERawAdvertisementsResponse *>(self);
  uint32_t size = 0;
  for (uint16_t i = 0; i < msg.advertisements_len; i++) {
    auto &sub_msg = msg.advertisements[i];
    size += 2;
    size += ProtoSize::calc_uint64_48bit_force(1, sub_msg.address);
    size += ProtoSize::calc_sint32_force(1, sub_msg.rssi);
    size += sub_msg.address_type ? 2 : 0;
    size += 2 + sub_msg.data_len;
  }
  return size;
}
#endif
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
bool BluetoothDeviceRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->address = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->request_type = static_cast<enums::BluetoothDeviceRequestType>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->has_address_type = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->address_type = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *BluetoothDeviceConnectionResponse::encode_msg(const void *self,
                                                       ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothDeviceConnectionResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.connected);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.mtu);
  pos = ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.error);
  return pos;
}
uint32_t BluetoothDeviceConnectionResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothDeviceConnectionResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  size += ProtoSize::calc_bool(1, msg.connected);
  size += ProtoSize::calc_uint32(1, msg.mtu);
  size += ProtoSize::calc_int32(1, msg.error);
  return size;
}
bool BluetoothGATTGetServicesRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->address = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *BluetoothGATTDescriptor::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothGATTDescriptor *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  if (msg.uuid[0] != 0 || msg.uuid[1] != 0) {
    pos = ProtoEncode::encode_uint64_force(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.uuid[0]);
    pos = ProtoEncode::encode_uint64_force(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.uuid[1]);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.handle);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.short_uuid);
  return pos;
}
uint32_t BluetoothGATTDescriptor::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothGATTDescriptor *>(self);
  uint32_t size = 0;
  if (msg.uuid[0] != 0 || msg.uuid[1] != 0) {
    size += ProtoSize::calc_uint64_force(1, msg.uuid[0]);
    size += ProtoSize::calc_uint64_force(1, msg.uuid[1]);
  }
  size += ProtoSize::calc_uint32(1, msg.handle);
  size += ProtoSize::calc_uint32(1, msg.short_uuid);
  return size;
}
uint8_t *BluetoothGATTCharacteristic::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothGATTCharacteristic *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  if (msg.uuid[0] != 0 || msg.uuid[1] != 0) {
    pos = ProtoEncode::encode_uint64_force(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.uuid[0]);
    pos = ProtoEncode::encode_uint64_force(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.uuid[1]);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.handle);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.properties);
  for (auto &it : msg.descriptors) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 4, it);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.short_uuid);
  return pos;
}
uint32_t BluetoothGATTCharacteristic::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothGATTCharacteristic *>(self);
  uint32_t size = 0;
  if (msg.uuid[0] != 0 || msg.uuid[1] != 0) {
    size += ProtoSize::calc_uint64_force(1, msg.uuid[0]);
    size += ProtoSize::calc_uint64_force(1, msg.uuid[1]);
  }
  size += ProtoSize::calc_uint32(1, msg.handle);
  size += ProtoSize::calc_uint32(1, msg.properties);
  if (!msg.descriptors.empty()) {
    for (const auto &it : msg.descriptors) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  size += ProtoSize::calc_uint32(1, msg.short_uuid);
  return size;
}
uint8_t *BluetoothGATTService::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothGATTService *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  if (msg.uuid[0] != 0 || msg.uuid[1] != 0) {
    pos = ProtoEncode::encode_uint64_force(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.uuid[0]);
    pos = ProtoEncode::encode_uint64_force(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.uuid[1]);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.handle);
  for (auto &it : msg.characteristics) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 3, it);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.short_uuid);
  return pos;
}
uint32_t BluetoothGATTService::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothGATTService *>(self);
  uint32_t size = 0;
  if (msg.uuid[0] != 0 || msg.uuid[1] != 0) {
    size += ProtoSize::calc_uint64_force(1, msg.uuid[0]);
    size += ProtoSize::calc_uint64_force(1, msg.uuid[1]);
  }
  size += ProtoSize::calc_uint32(1, msg.handle);
  if (!msg.characteristics.empty()) {
    for (const auto &it : msg.characteristics) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  size += ProtoSize::calc_uint32(1, msg.short_uuid);
  return size;
}
uint8_t *BluetoothGATTGetServicesResponse::encode_msg(const void *self,
                                                      ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothGATTGetServicesResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  for (auto &it : msg.services) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 2, it);
  }
  return pos;
}
uint32_t BluetoothGATTGetServicesResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothGATTGetServicesResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  if (!msg.services.empty()) {
    for (const auto &it : msg.services) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  return size;
}
uint8_t *BluetoothGATTGetServicesDoneResponse::encode_msg(const void *self,
                                                          ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothGATTGetServicesDoneResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  return pos;
}
uint32_t BluetoothGATTGetServicesDoneResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothGATTGetServicesDoneResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  return size;
}
bool BluetoothGATTReadRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->address = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->handle = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *BluetoothGATTReadResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothGATTReadResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.handle);
  pos = ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.data_ptr_, msg.data_len_);
  return pos;
}
uint32_t BluetoothGATTReadResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothGATTReadResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  size += ProtoSize::calc_uint32(1, msg.handle);
  size += ProtoSize::calc_length(1, msg.data_len_);
  return size;
}
bool BluetoothGATTWriteRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->address = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->handle = value.as_varint();
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->response = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(4, 2):
      PROTO_DECODE_GUARD(tag, 4, 2);
      this->data = value.data();
      this->data_len = value.size();
      break;
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTReadDescriptorRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->address = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->handle = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTWriteDescriptorRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->address = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->handle = value.as_varint();
      break;
    case PROTO_DECODE_CASE(3, 2):
      PROTO_DECODE_GUARD(tag, 3, 2);
      this->data = value.data();
      this->data_len = value.size();
      break;
    default:
      return false;
  }
  return true;
}
bool BluetoothGATTNotifyRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->address = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->handle = value.as_varint();
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->enable = value.as_varint() != 0;
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *BluetoothGATTNotifyDataResponse::encode_msg(const void *self,
                                                     ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothGATTNotifyDataResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.handle);
  pos = ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.data_ptr_, msg.data_len_);
  return pos;
}
uint32_t BluetoothGATTNotifyDataResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothGATTNotifyDataResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  size += ProtoSize::calc_uint32(1, msg.handle);
  size += ProtoSize::calc_length(1, msg.data_len_);
  return size;
}
uint8_t *BluetoothConnectionsFreeResponse::encode_msg(const void *self,
                                                      ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothConnectionsFreeResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.free);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.limit);
  for (const auto &it : msg.allocated) {
    if (it != 0) {
      pos = ProtoEncode::encode_uint64_force(pos PROTO_ENCODE_DEBUG_ARG, 3, it);
    }
  }
  return pos;
}
uint32_t BluetoothConnectionsFreeResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothConnectionsFreeResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.free);
  size += ProtoSize::calc_uint32(1, msg.limit);
  for (const auto &it : msg.allocated) {
    if (it != 0) {
      size += ProtoSize::calc_uint64_force(1, it);
    }
  }
  return size;
}
uint8_t *BluetoothGATTErrorResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothGATTErrorResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.handle);
  pos = ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.error);
  return pos;
}
uint32_t BluetoothGATTErrorResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothGATTErrorResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  size += ProtoSize::calc_uint32(1, msg.handle);
  size += ProtoSize::calc_int32(1, msg.error);
  return size;
}
uint8_t *BluetoothGATTWriteResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothGATTWriteResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.handle);
  return pos;
}
uint32_t BluetoothGATTWriteResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothGATTWriteResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  size += ProtoSize::calc_uint32(1, msg.handle);
  return size;
}
uint8_t *BluetoothGATTNotifyResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothGATTNotifyResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.handle);
  return pos;
}
uint32_t BluetoothGATTNotifyResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothGATTNotifyResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  size += ProtoSize::calc_uint32(1, msg.handle);
  return size;
}
uint8_t *BluetoothDevicePairingResponse::encode_msg(const void *self,
                                                    ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothDevicePairingResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.paired);
  pos = ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.error);
  return pos;
}
uint32_t BluetoothDevicePairingResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothDevicePairingResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  size += ProtoSize::calc_bool(1, msg.paired);
  size += ProtoSize::calc_int32(1, msg.error);
  return size;
}
uint8_t *BluetoothDeviceUnpairingResponse::encode_msg(const void *self,
                                                      ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothDeviceUnpairingResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.success);
  pos = ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.error);
  return pos;
}
uint32_t BluetoothDeviceUnpairingResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothDeviceUnpairingResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  size += ProtoSize::calc_bool(1, msg.success);
  size += ProtoSize::calc_int32(1, msg.error);
  return size;
}
uint8_t *BluetoothDeviceClearCacheResponse::encode_msg(const void *self,
                                                       ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothDeviceClearCacheResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.success);
  pos = ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.error);
  return pos;
}
uint32_t BluetoothDeviceClearCacheResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothDeviceClearCacheResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  size += ProtoSize::calc_bool(1, msg.success);
  size += ProtoSize::calc_int32(1, msg.error);
  return size;
}
#endif
#ifdef USE_BLUETOOTH_PROXY
uint8_t *BluetoothScannerStateResponse::encode_msg(const void *self,
                                                   ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothScannerStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, static_cast<uint32_t>(msg.state));
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(msg.mode));
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, static_cast<uint32_t>(msg.configured_mode));
  return pos;
}
uint32_t BluetoothScannerStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothScannerStateResponse *>(self);
  uint32_t size = 0;
  size += msg.state ? 2 : 0;
  size += msg.mode ? 2 : 0;
  size += msg.configured_mode ? 2 : 0;
  return size;
}
bool BluetoothScannerSetModeRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->mode = static_cast<enums::BluetoothScannerMode>(value.as_varint());
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_VOICE_ASSISTANT
bool SubscribeVoiceAssistantRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->subscribe = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->flags = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *VoiceAssistantAudioSettings::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const VoiceAssistantAudioSettings *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.noise_suppression_level);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.auto_gain);
  if (uint32_t raw = float_to_raw(msg.volume_multiplier); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 29, raw);
  }
  return pos;
}
uint32_t VoiceAssistantAudioSettings::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const VoiceAssistantAudioSettings *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.noise_suppression_level);
  size += ProtoSize::calc_uint32(1, msg.auto_gain);
  size += ProtoSize::calc_float(1, msg.volume_multiplier);
  return size;
}
uint8_t *VoiceAssistantRequest::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const VoiceAssistantRequest *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.start);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.conversation_id);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.flags);
  pos = ProtoEncode::encode_optional_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 4, msg.audio_settings);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.wake_word_phrase);
  return pos;
}
uint32_t VoiceAssistantRequest::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const VoiceAssistantRequest *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_bool(1, msg.start);
  size += ProtoSize::calc_length(1, msg.conversation_id.size());
  size += ProtoSize::calc_uint32(1, msg.flags);
  size += ProtoSize::calc_message(1, msg.audio_settings.calculate_size());
  size += ProtoSize::calc_length(1, msg.wake_word_phrase.size());
  return size;
}
bool VoiceAssistantResponse::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->port = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->error = value.as_varint() != 0;
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantEventData::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 2):
      PROTO_DECODE_GUARD(tag, 1, 2);
      this->name = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(2, 2):
      PROTO_DECODE_GUARD(tag, 2, 2);
      this->value = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantEventResponse::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->event_type = static_cast<enums::VoiceAssistantEvent>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(2, 2):
      PROTO_DECODE_GUARD(tag, 2, 2);
      this->data.emplace_back();
      value.decode_to_message(this->data.back());
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantAudio::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 2):
      PROTO_DECODE_GUARD(tag, 1, 2);
      this->data = value.data();
      this->data_len = value.size();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->end = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(3, 2):
      PROTO_DECODE_GUARD(tag, 3, 2);
      this->data2 = value.data();
      this->data2_len = value.size();
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *VoiceAssistantAudio::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const VoiceAssistantAudio *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.data, msg.data_len);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.end);
  pos = ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.data2, msg.data2_len);
  return pos;
}
uint32_t VoiceAssistantAudio::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const VoiceAssistantAudio *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, msg.data_len);
  size += ProtoSize::calc_bool(1, msg.end);
  size += ProtoSize::calc_length(1, msg.data2_len);
  return size;
}
bool VoiceAssistantTimerEventResponse::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->event_type = static_cast<enums::VoiceAssistantTimerEvent>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(2, 2):
      PROTO_DECODE_GUARD(tag, 2, 2);
      this->timer_id = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(3, 2):
      PROTO_DECODE_GUARD(tag, 3, 2);
      this->name = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->total_seconds = value.as_varint();
      break;
    case PROTO_DECODE_CASE(5, 0):
      PROTO_DECODE_GUARD(tag, 5, 0);
      this->seconds_left = value.as_varint();
      break;
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->is_active = value.as_varint() != 0;
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantAnnounceRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 2):
      PROTO_DECODE_GUARD(tag, 1, 2);
      this->media_id = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(2, 2):
      PROTO_DECODE_GUARD(tag, 2, 2);
      this->text = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(3, 2):
      PROTO_DECODE_GUARD(tag, 3, 2);
      this->preannounce_media_id = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->start_conversation = value.as_varint() != 0;
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *VoiceAssistantAnnounceFinished::encode_msg(const void *self,
                                                    ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const VoiceAssistantAnnounceFinished *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.success);
  return pos;
}
uint32_t VoiceAssistantAnnounceFinished::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const VoiceAssistantAnnounceFinished *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_bool(1, msg.success);
  return size;
}
uint8_t *VoiceAssistantWakeWord::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const VoiceAssistantWakeWord *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.id);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.wake_word);
  for (auto &it : msg.trained_languages) {
    pos = ProtoEncode::encode_string_force(pos PROTO_ENCODE_DEBUG_ARG, 3, it);
  }
  return pos;
}
uint32_t VoiceAssistantWakeWord::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const VoiceAssistantWakeWord *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, msg.id.size());
  size += ProtoSize::calc_length(1, msg.wake_word.size());
  if (!msg.trained_languages.empty()) {
    for (const auto &it : msg.trained_languages) {
      size += ProtoSize::calc_length_force(1, it.size());
    }
  }
  return size;
}
bool VoiceAssistantExternalWakeWord::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 2):
      PROTO_DECODE_GUARD(tag, 1, 2);
      this->id = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(2, 2):
      PROTO_DECODE_GUARD(tag, 2, 2);
      this->wake_word = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(3, 2):
      PROTO_DECODE_GUARD(tag, 3, 2);
      this->trained_languages.push_back(value.as_string());
      break;
    case PROTO_DECODE_CASE(4, 2):
      PROTO_DECODE_GUARD(tag, 4, 2);
      this->model_type = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(5, 0):
      PROTO_DECODE_GUARD(tag, 5, 0);
      this->model_size = value.as_varint();
      break;
    case PROTO_DECODE_CASE(6, 2):
      PROTO_DECODE_GUARD(tag, 6, 2);
      this->model_hash = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    case PROTO_DECODE_CASE(7, 2):
      PROTO_DECODE_GUARD(tag, 7, 2);
      this->url = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
    default:
      return false;
  }
  return true;
}
bool VoiceAssistantConfigurationRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 2):
      PROTO_DECODE_GUARD(tag, 1, 2);
      this->external_wake_words.emplace_back();
      value.decode_to_message(this->external_wake_words.back());
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *VoiceAssistantConfigurationResponse::encode_msg(const void *self,
                                                         ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const VoiceAssistantConfigurationResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  for (auto &it : msg.available_wake_words) {
    pos = ProtoEncode::encode_sub_message(pos PROTO_ENCODE_DEBUG_ARG, buffer, 1, it);
  }
  for (const auto &it : *msg.active_wake_words) {
    pos = ProtoEncode::encode_string_force(pos PROTO_ENCODE_DEBUG_ARG, 2, it);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.max_active_wake_words);
  return pos;
}
uint32_t VoiceAssistantConfigurationResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const VoiceAssistantConfigurationResponse *>(self);
  uint32_t size = 0;
  if (!msg.available_wake_words.empty()) {
    for (const auto &it : msg.available_wake_words) {
      size += ProtoSize::calc_message_force(1, it.calculate_size());
    }
  }
  if (!msg.active_wake_words->empty()) {
    for (const auto &it : *msg.active_wake_words) {
      size += ProtoSize::calc_length_force(1, it.size());
    }
  }
  size += ProtoSize::calc_uint32(1, msg.max_active_wake_words);
  return size;
}
bool VoiceAssistantSetConfiguration::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 2):
      PROTO_DECODE_GUARD(tag, 1, 2);
      this->active_wake_words.push_back(value.as_string());
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
uint8_t *ListEntitiesAlarmControlPanelResponse::encode_msg(const void *self,
                                                           ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesAlarmControlPanelResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.supported_features);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.requires_code);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.requires_code_to_arm);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesAlarmControlPanelResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesAlarmControlPanelResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
  size += ProtoSize::calc_uint32(1, msg.supported_features);
  size += ProtoSize::calc_bool(1, msg.requires_code);
  size += ProtoSize::calc_bool(1, msg.requires_code_to_arm);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *AlarmControlPanelStateResponse::encode_msg(const void *self,
                                                    ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const AlarmControlPanelStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(msg.state));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.device_id);
#endif
  return pos;
}
uint32_t AlarmControlPanelStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const AlarmControlPanelStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += msg.state ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool AlarmControlPanelCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->command = static_cast<enums::AlarmControlPanelStateCommand>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(3, 2):
      PROTO_DECODE_GUARD(tag, 3, 2);
      this->code = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_TEXT
uint8_t *ListEntitiesTextResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesTextResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.min_length);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.max_length);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.pattern);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, static_cast<uint32_t>(msg.mode));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesTextResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesTextResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
  size += ProtoSize::calc_uint32(1, msg.min_length);
  size += ProtoSize::calc_uint32(1, msg.max_length);
  size += ProtoSize::calc_length(1, msg.pattern.size());
  size += msg.mode ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *TextStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const TextStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.state);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.missing_state);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.device_id);
#endif
  return pos;
}
uint32_t TextStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const TextStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_length(1, msg.state.size());
  size += ProtoSize::calc_bool(1, msg.missing_state);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool TextCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 2):
      PROTO_DECODE_GUARD(tag, 2, 2);
      this->state = StringRef(reinterpret_cast<const char *>(value.data()), value.size());
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_DATETIME_DATE
uint8_t *ListEntitiesDateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesDateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesDateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesDateResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *DateStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const DateStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.missing_state);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.year);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.month);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.day);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.device_id);
#endif
  return pos;
}
uint32_t DateStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const DateStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, msg.missing_state);
  size += ProtoSize::calc_uint32(1, msg.year);
  size += ProtoSize::calc_uint32(1, msg.month);
  size += ProtoSize::calc_uint32(1, msg.day);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool DateCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->year = value.as_varint();
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->month = value.as_varint();
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->day = value.as_varint();
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(5, 0):
      PROTO_DECODE_GUARD(tag, 5, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_DATETIME_TIME
uint8_t *ListEntitiesTimeResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesTimeResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesTimeResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesTimeResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *TimeStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const TimeStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.missing_state);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.hour);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.minute);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.second);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.device_id);
#endif
  return pos;
}
uint32_t TimeStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const TimeStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, msg.missing_state);
  size += ProtoSize::calc_uint32(1, msg.hour);
  size += ProtoSize::calc_uint32(1, msg.minute);
  size += ProtoSize::calc_uint32(1, msg.second);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool TimeCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->hour = value.as_varint();
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->minute = value.as_varint();
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->second = value.as_varint();
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(5, 0):
      PROTO_DECODE_GUARD(tag, 5, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_EVENT
uint8_t *ListEntitiesEventResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesEventResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.device_class);
  for (const char *it : *msg.event_types) {
    pos = ProtoEncode::encode_string_force(pos PROTO_ENCODE_DEBUG_ARG, 9, it, strlen(it));
  }
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesEventResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesEventResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
  size += !msg.device_class.empty() ? 2 + msg.device_class.size() : 0;
  if (!msg.event_types->empty()) {
    for (const char *it : *msg.event_types) {
      size += ProtoSize::calc_length_force(1, strlen(it));
    }
  }
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *EventResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const EventResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.event_type);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.device_id);
#endif
  return pos;
}
uint32_t EventResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const EventResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_length(1, msg.event_type.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
#endif
#ifdef USE_VALVE
uint8_t *ListEntitiesValveResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesValveResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.device_class);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.assumed_state);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.supports_position);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 11, msg.supports_stop);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 12, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesValveResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesValveResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
  size += !msg.device_class.empty() ? 2 + msg.device_class.size() : 0;
  size += ProtoSize::calc_bool(1, msg.assumed_state);
  size += ProtoSize::calc_bool(1, msg.supports_position);
  size += ProtoSize::calc_bool(1, msg.supports_stop);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *ValveStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ValveStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  if (uint32_t raw = float_to_raw(msg.position); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, raw);
  }
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, static_cast<uint32_t>(msg.current_operation));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.device_id);
#endif
  return pos;
}
uint32_t ValveStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ValveStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_float(1, msg.position);
  size += msg.current_operation ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool ValveCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->has_position = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(3, 5):
      PROTO_DECODE_GUARD(tag, 3, 5);
      this->position = value.as_float();
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->stop = value.as_varint() != 0;
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(5, 0):
      PROTO_DECODE_GUARD(tag, 5, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_DATETIME_DATETIME
uint8_t *ListEntitiesDateTimeResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesDateTimeResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesDateTimeResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesDateTimeResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *DateTimeStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const DateTimeStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.missing_state);
  if (uint32_t raw = msg.epoch_seconds; raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 29, raw);
  }
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.device_id);
#endif
  return pos;
}
uint32_t DateTimeStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const DateTimeStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, msg.missing_state);
  size += ProtoSize::calc_fixed32(1, msg.epoch_seconds);
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool DateTimeCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 5):
      PROTO_DECODE_GUARD(tag, 2, 5);
      this->epoch_seconds = value.as_fixed32();
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_UPDATE
uint8_t *ListEntitiesUpdateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesUpdateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, static_cast<uint32_t>(msg.entity_category));
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.device_class);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.device_id);
#endif
  return pos;
}
uint32_t ListEntitiesUpdateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesUpdateResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
  size += !msg.device_class.empty() ? 2 + msg.device_class.size() : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
uint8_t *UpdateStateResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const UpdateStateResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 13, msg.key);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.missing_state);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 3, msg.in_progress);
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.has_progress);
  if (uint32_t raw = float_to_raw(msg.progress); raw != 0) [[likely]] {
    pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 45, raw);
  }
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 6, msg.current_version);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.latest_version);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.title);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.release_summary);
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.release_url);
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, msg.device_id);
#endif
  return pos;
}
uint32_t UpdateStateResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const UpdateStateResponse *>(self);
  uint32_t size = 0;
  size += 5;
  size += ProtoSize::calc_bool(1, msg.missing_state);
  size += ProtoSize::calc_bool(1, msg.in_progress);
  size += ProtoSize::calc_bool(1, msg.has_progress);
  size += ProtoSize::calc_float(1, msg.progress);
  size += ProtoSize::calc_length(1, msg.current_version.size());
  size += ProtoSize::calc_length(1, msg.latest_version.size());
  size += ProtoSize::calc_length(1, msg.title.size());
  size += ProtoSize::calc_length(1, msg.release_summary.size());
  size += ProtoSize::calc_length(1, msg.release_url.size());
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  return size;
}
bool UpdateCommandRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 5):
      PROTO_DECODE_GUARD(tag, 1, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->command = static_cast<enums::UpdateCommand>(value.as_varint());
      break;
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->device_id = value.as_varint();
      break;
#endif
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_ZWAVE_PROXY
bool ZWaveProxyFrame::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 2):
      PROTO_DECODE_GUARD(tag, 1, 2);
      this->data = value.data();
      this->data_len = value.size();
      break;
    default:
      return false;
  }
  return true;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
ZWaveProxyFrame::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ZWaveProxyFrame *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.data, msg.data_len);
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
ZWaveProxyFrame::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ZWaveProxyFrame *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_length(1, msg.data_len);
  return size;
}
bool ZWaveProxyRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->type = static_cast<enums::ZWaveProxyRequestType>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(2, 2):
      PROTO_DECODE_GUARD(tag, 2, 2);
      this->data = value.data();
      this->data_len = value.size();
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *ZWaveProxyRequest::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ZWaveProxyRequest *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, static_cast<uint32_t>(msg.type));
  pos = ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.data, msg.data_len);
  return pos;
}
uint32_t ZWaveProxyRequest::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ZWaveProxyRequest *>(self);
  uint32_t size = 0;
  size += msg.type ? 2 : 0;
  size += ProtoSize::calc_length(1, msg.data_len);
  return size;
}
uint8_t *ZWaveProxyRequestResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ZWaveProxyRequestResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, static_cast<uint32_t>(msg.type));
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(msg.status));
  return pos;
}
uint32_t ZWaveProxyRequestResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ZWaveProxyRequestResponse *>(self);
  uint32_t size = 0;
  size += msg.type ? 2 : 0;
  size += msg.status ? 2 : 0;
  return size;
}
#endif
#ifdef USE_INFRARED
uint8_t *ListEntitiesInfraredResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesInfraredResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.device_id);
#endif
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.capabilities);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.receiver_frequency);
  return pos;
}
uint32_t ListEntitiesInfraredResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesInfraredResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  size += ProtoSize::calc_uint32(1, msg.capabilities);
  size += ProtoSize::calc_uint32(1, msg.receiver_frequency);
  return size;
}
#endif
#if defined(USE_IR_RF) || defined(USE_RADIO_FREQUENCY)
bool InfraredRFTransmitRawTimingsRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
#ifdef USE_DEVICES
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->device_id = value.as_varint();
      break;
#endif
    case PROTO_DECODE_CASE(2, 5):
      PROTO_DECODE_GUARD(tag, 2, 5);
      this->key = value.as_fixed32();
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->carrier_frequency = value.as_varint();
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->repeat_count = value.as_varint();
      break;
    case PROTO_DECODE_CASE(5, 2):
      PROTO_DECODE_GUARD(tag, 5, 2);
      this->timings_data_ = value.data();
      this->timings_length_ = value.size();
      this->timings_count_ = count_packed_varints(value.data(), value.size());
      break;
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->modulation = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
InfraredRFReceiveEvent::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const InfraredRFReceiveEvent *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.device_id);
#endif
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  for (const auto &it : *msg.timings) {
    pos = ProtoEncode::encode_sint32_force(pos PROTO_ENCODE_DEBUG_ARG, 3, it);
  }
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
InfraredRFReceiveEvent::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const InfraredRFReceiveEvent *>(self);
  uint32_t size = 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  size += 5;
  if (!msg.timings->empty()) {
    for (const auto &it : *msg.timings) {
      size += ProtoSize::calc_sint32_force(1, it);
    }
  }
  return size;
}
#endif
#ifdef USE_RADIO_FREQUENCY
uint8_t *ListEntitiesRadioFrequencyResponse::encode_msg(const void *self,
                                                        ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const ListEntitiesRadioFrequencyResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.object_id);
  pos = ProtoEncode::write_tag_and_fixed32(pos PROTO_ENCODE_DEBUG_ARG, 21, msg.key);
  pos = ProtoEncode::encode_short_string_force(pos PROTO_ENCODE_DEBUG_ARG, 26, msg.name);
#ifdef USE_ENTITY_ICON
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.icon);
#endif
  pos = ProtoEncode::encode_bool(pos PROTO_ENCODE_DEBUG_ARG, 5, msg.disabled_by_default);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 6, static_cast<uint32_t>(msg.entity_category));
#ifdef USE_DEVICES
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 7, msg.device_id);
#endif
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 8, msg.capabilities);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 9, msg.frequency_min);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 10, msg.frequency_max);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 11, msg.supported_modulations);
  return pos;
}
uint32_t ListEntitiesRadioFrequencyResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const ListEntitiesRadioFrequencyResponse *>(self);
  uint32_t size = 0;
  size += 2 + msg.object_id.size();
  size += 5;
  size += 2 + msg.name.size();
#ifdef USE_ENTITY_ICON
  size += !msg.icon.empty() ? 2 + msg.icon.size() : 0;
#endif
  size += ProtoSize::calc_bool(1, msg.disabled_by_default);
  size += msg.entity_category ? 2 : 0;
#ifdef USE_DEVICES
  size += ProtoSize::calc_uint32(1, msg.device_id);
#endif
  size += ProtoSize::calc_uint32(1, msg.capabilities);
  size += ProtoSize::calc_uint32(1, msg.frequency_min);
  size += ProtoSize::calc_uint32(1, msg.frequency_max);
  size += ProtoSize::calc_uint32(1, msg.supported_modulations);
  return size;
}
#endif
#ifdef USE_SERIAL_PROXY
bool SerialProxyConfigureRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->instance = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->baudrate = value.as_varint();
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->flow_control = value.as_varint() != 0;
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->parity = static_cast<enums::SerialProxyParity>(value.as_varint());
      break;
    case PROTO_DECODE_CASE(5, 0):
      PROTO_DECODE_GUARD(tag, 5, 0);
      this->stop_bits = value.as_varint();
      break;
    case PROTO_DECODE_CASE(6, 0):
      PROTO_DECODE_GUARD(tag, 6, 0);
      this->data_size = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint8_t *
SerialProxyDataReceived::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const SerialProxyDataReceived *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.instance);
  pos = ProtoEncode::encode_bytes(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.data_ptr_, msg.data_len_);
  return pos;
}
__attribute__((optimize("O2")))  // NOLINT(clang-diagnostic-unknown-attributes)
uint32_t
SerialProxyDataReceived::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const SerialProxyDataReceived *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.instance);
  size += ProtoSize::calc_length(1, msg.data_len_);
  return size;
}
bool SerialProxyWriteRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->instance = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 2):
      PROTO_DECODE_GUARD(tag, 2, 2);
      this->data = value.data();
      this->data_len = value.size();
      break;
    default:
      return false;
  }
  return true;
}
bool SerialProxySetModemPinsRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->instance = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->line_states = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
bool SerialProxyGetModemPinsRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->instance = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *SerialProxyGetModemPinsResponse::encode_msg(const void *self,
                                                     ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const SerialProxyGetModemPinsResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.instance);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.line_states);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, static_cast<uint32_t>(msg.status));
  return pos;
}
uint32_t SerialProxyGetModemPinsResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const SerialProxyGetModemPinsResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.instance);
  size += ProtoSize::calc_uint32(1, msg.line_states);
  size += msg.status ? 2 : 0;
  return size;
}
bool SerialProxyRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->instance = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->type = static_cast<enums::SerialProxyRequestType>(value.as_varint());
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *SerialProxyRequestResponse::encode_msg(const void *self, ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const SerialProxyRequestResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.instance);
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 2, static_cast<uint32_t>(msg.type));
  pos = ProtoEncode::encode_uint32(pos PROTO_ENCODE_DEBUG_ARG, 3, static_cast<uint32_t>(msg.status));
  pos = ProtoEncode::encode_string(pos PROTO_ENCODE_DEBUG_ARG, 4, msg.error_message);
  return pos;
}
uint32_t SerialProxyRequestResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const SerialProxyRequestResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint32(1, msg.instance);
  size += msg.type ? 2 : 0;
  size += msg.status ? 2 : 0;
  size += ProtoSize::calc_length(1, msg.error_message.size());
  return size;
}
bool SerialProxySetModeRequest::decode_varint(uint32_t field_id, proto_varint_value_t value) {
  switch (field_id) {
    case 1:
      this->instance = value;
      break;
    case 2:
      this->mode = static_cast<enums::SerialProxyMode>(value);
      break;
    default:
      return false;
  }
  return true;
}
#endif
#ifdef USE_BLUETOOTH_PROXY_CONNECTIONS
bool BluetoothSetConnectionParamsRequest::decode_field(uint32_t tag, const uint8_t *data, proto_varint_value_t scalar) {
  const ProtoFieldValue value(data, scalar);
  switch (PROTO_DECODE_KEY(tag)) {
    case PROTO_DECODE_CASE(1, 0):
      PROTO_DECODE_GUARD(tag, 1, 0);
      this->address = value.as_varint();
      break;
    case PROTO_DECODE_CASE(2, 0):
      PROTO_DECODE_GUARD(tag, 2, 0);
      this->min_interval = value.as_varint();
      break;
    case PROTO_DECODE_CASE(3, 0):
      PROTO_DECODE_GUARD(tag, 3, 0);
      this->max_interval = value.as_varint();
      break;
    case PROTO_DECODE_CASE(4, 0):
      PROTO_DECODE_GUARD(tag, 4, 0);
      this->latency = value.as_varint();
      break;
    case PROTO_DECODE_CASE(5, 0):
      PROTO_DECODE_GUARD(tag, 5, 0);
      this->timeout = value.as_varint();
      break;
    default:
      return false;
  }
  return true;
}
uint8_t *BluetoothSetConnectionParamsResponse::encode_msg(const void *self,
                                                          ProtoWriteBuffer &buffer PROTO_ENCODE_DEBUG_PARAM) {
  const auto &msg = *static_cast<const BluetoothSetConnectionParamsResponse *>(self);
  uint8_t *__restrict__ pos = buffer.get_pos();
  pos = ProtoEncode::encode_uint64(pos PROTO_ENCODE_DEBUG_ARG, 1, msg.address);
  pos = ProtoEncode::encode_int32(pos PROTO_ENCODE_DEBUG_ARG, 2, msg.error);
  return pos;
}
uint32_t BluetoothSetConnectionParamsResponse::calc_size_msg(const void *self) {
  const auto &msg = *static_cast<const BluetoothSetConnectionParamsResponse *>(self);
  uint32_t size = 0;
  size += ProtoSize::calc_uint64(1, msg.address);
  size += ProtoSize::calc_int32(1, msg.error);
  return size;
}
#endif

}  // namespace esphome::api
