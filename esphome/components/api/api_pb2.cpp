// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#include "api_pb2.h"
#include "api_pb2_size.h"
#include "esphome/core/log.h"
#include "esphome/core/helpers.h"

namespace esphome {
namespace api {

bool HelloRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->api_version_major = value.as_uint32();
      return true;
    }
    case 3: {
      this->api_version_minor = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool HelloRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->client_info = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void HelloRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->client_info);
  buffer.encode_uint32(2, this->api_version_major);
  buffer.encode_uint32(3, this->api_version_minor);
}
void HelloRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->client_info, false);
  ProtoSize::add_uint32_field(total_size, 1, this->api_version_major, false);
  ProtoSize::add_uint32_field(total_size, 1, this->api_version_minor, false);
}
bool HelloResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->api_version_major = value.as_uint32();
      return true;
    }
    case 2: {
      this->api_version_minor = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool HelloResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3: {
      this->server_info = value.as_string();
      return true;
    }
    case 4: {
      this->name = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void HelloResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->api_version_major);
  buffer.encode_uint32(2, this->api_version_minor);
  buffer.encode_string(3, this->server_info);
  buffer.encode_string(4, this->name);
}
void HelloResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint32_field(total_size, 1, this->api_version_major, false);
  ProtoSize::add_uint32_field(total_size, 1, this->api_version_minor, false);
  ProtoSize::add_string_field(total_size, 1, this->server_info, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
}
bool ConnectRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->password = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void ConnectRequest::encode(ProtoWriteBuffer buffer) const { buffer.encode_string(1, this->password); }
void ConnectRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->password, false);
}
bool ConnectResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->invalid_password = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
void ConnectResponse::encode(ProtoWriteBuffer buffer) const { buffer.encode_bool(1, this->invalid_password); }
void ConnectResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_bool_field(total_size, 1, this->invalid_password, false);
}
bool AreaInfo::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->area_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool AreaInfo::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->name = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void AreaInfo::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->area_id);
  buffer.encode_string(2, this->name);
}
void AreaInfo::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint32_field(total_size, 1, this->area_id, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
}
bool DeviceInfo::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->device_id = value.as_uint32();
      return true;
    }
    case 3: {
      this->area_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool DeviceInfo::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->name = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void DeviceInfo::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->device_id);
  buffer.encode_string(2, this->name);
  buffer.encode_uint32(3, this->area_id);
}
void DeviceInfo::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_uint32_field(total_size, 1, this->area_id, false);
}
bool DeviceInfoResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->uses_password = value.as_bool();
      return true;
    }
    case 7: {
      this->has_deep_sleep = value.as_bool();
      return true;
    }
    case 10: {
      this->webserver_port = value.as_uint32();
      return true;
    }
    case 11: {
      this->legacy_bluetooth_proxy_version = value.as_uint32();
      return true;
    }
    case 15: {
      this->bluetooth_proxy_feature_flags = value.as_uint32();
      return true;
    }
    case 14: {
      this->legacy_voice_assistant_version = value.as_uint32();
      return true;
    }
    case 17: {
      this->voice_assistant_feature_flags = value.as_uint32();
      return true;
    }
    case 19: {
      this->api_encryption_supported = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool DeviceInfoResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->name = value.as_string();
      return true;
    }
    case 3: {
      this->mac_address = value.as_string();
      return true;
    }
    case 4: {
      this->esphome_version = value.as_string();
      return true;
    }
    case 5: {
      this->compilation_time = value.as_string();
      return true;
    }
    case 6: {
      this->model = value.as_string();
      return true;
    }
    case 8: {
      this->project_name = value.as_string();
      return true;
    }
    case 9: {
      this->project_version = value.as_string();
      return true;
    }
    case 12: {
      this->manufacturer = value.as_string();
      return true;
    }
    case 13: {
      this->friendly_name = value.as_string();
      return true;
    }
    case 16: {
      this->suggested_area = value.as_string();
      return true;
    }
    case 18: {
      this->bluetooth_mac_address = value.as_string();
      return true;
    }
    case 20: {
      this->devices.push_back(value.as_message<DeviceInfo>());
      return true;
    }
    case 21: {
      this->areas.push_back(value.as_message<AreaInfo>());
      return true;
    }
    case 22: {
      this->area = value.as_message<AreaInfo>();
      return true;
    }
    default:
      return false;
  }
}
void DeviceInfoResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_bool(1, this->uses_password);
  buffer.encode_string(2, this->name);
  buffer.encode_string(3, this->mac_address);
  buffer.encode_string(4, this->esphome_version);
  buffer.encode_string(5, this->compilation_time);
  buffer.encode_string(6, this->model);
  buffer.encode_bool(7, this->has_deep_sleep);
  buffer.encode_string(8, this->project_name);
  buffer.encode_string(9, this->project_version);
  buffer.encode_uint32(10, this->webserver_port);
  buffer.encode_uint32(11, this->legacy_bluetooth_proxy_version);
  buffer.encode_uint32(15, this->bluetooth_proxy_feature_flags);
  buffer.encode_string(12, this->manufacturer);
  buffer.encode_string(13, this->friendly_name);
  buffer.encode_uint32(14, this->legacy_voice_assistant_version);
  buffer.encode_uint32(17, this->voice_assistant_feature_flags);
  buffer.encode_string(16, this->suggested_area);
  buffer.encode_string(18, this->bluetooth_mac_address);
  buffer.encode_bool(19, this->api_encryption_supported);
  for (auto &it : this->devices) {
    buffer.encode_message<DeviceInfo>(20, it, true);
  }
  for (auto &it : this->areas) {
    buffer.encode_message<AreaInfo>(21, it, true);
  }
  buffer.encode_message<AreaInfo>(22, this->area);
}
void DeviceInfoResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_bool_field(total_size, 1, this->uses_password, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->mac_address, false);
  ProtoSize::add_string_field(total_size, 1, this->esphome_version, false);
  ProtoSize::add_string_field(total_size, 1, this->compilation_time, false);
  ProtoSize::add_string_field(total_size, 1, this->model, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_deep_sleep, false);
  ProtoSize::add_string_field(total_size, 1, this->project_name, false);
  ProtoSize::add_string_field(total_size, 1, this->project_version, false);
  ProtoSize::add_uint32_field(total_size, 1, this->webserver_port, false);
  ProtoSize::add_uint32_field(total_size, 1, this->legacy_bluetooth_proxy_version, false);
  ProtoSize::add_uint32_field(total_size, 1, this->bluetooth_proxy_feature_flags, false);
  ProtoSize::add_string_field(total_size, 1, this->manufacturer, false);
  ProtoSize::add_string_field(total_size, 1, this->friendly_name, false);
  ProtoSize::add_uint32_field(total_size, 1, this->legacy_voice_assistant_version, false);
  ProtoSize::add_uint32_field(total_size, 2, this->voice_assistant_feature_flags, false);
  ProtoSize::add_string_field(total_size, 2, this->suggested_area, false);
  ProtoSize::add_string_field(total_size, 2, this->bluetooth_mac_address, false);
  ProtoSize::add_bool_field(total_size, 2, this->api_encryption_supported, false);
  ProtoSize::add_repeated_message(total_size, 2, this->devices);
  ProtoSize::add_repeated_message(total_size, 2, this->areas);
  ProtoSize::add_message_object(total_size, 2, this->area, false);
}
#ifdef USE_BINARY_SENSOR
bool ListEntitiesBinarySensorResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->is_status_binary_sensor = value.as_bool();
      return true;
    }
    case 7: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 9: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 10: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesBinarySensorResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->device_class = value.as_string();
      return true;
    }
    case 8: {
      this->icon = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesBinarySensorResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesBinarySensorResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->device_class);
  buffer.encode_bool(6, this->is_status_binary_sensor);
  buffer.encode_bool(7, this->disabled_by_default);
  buffer.encode_string(8, this->icon);
  buffer.encode_enum<enums::EntityCategory>(9, this->entity_category);
  buffer.encode_uint32(10, this->device_id);
}
void ListEntitiesBinarySensorResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->device_class, false);
  ProtoSize::add_bool_field(total_size, 1, this->is_status_binary_sensor, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool BinarySensorStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_bool();
      return true;
    }
    case 3: {
      this->missing_state = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool BinarySensorStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void BinarySensorStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
  buffer.encode_bool(3, this->missing_state);
}
void BinarySensorStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->state, false);
  ProtoSize::add_bool_field(total_size, 1, this->missing_state, false);
}
#endif
#ifdef USE_COVER
bool ListEntitiesCoverResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 5: {
      this->assumed_state = value.as_bool();
      return true;
    }
    case 6: {
      this->supports_position = value.as_bool();
      return true;
    }
    case 7: {
      this->supports_tilt = value.as_bool();
      return true;
    }
    case 9: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 11: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 12: {
      this->supports_stop = value.as_bool();
      return true;
    }
    case 13: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesCoverResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 8: {
      this->device_class = value.as_string();
      return true;
    }
    case 10: {
      this->icon = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesCoverResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesCoverResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_bool(5, this->assumed_state);
  buffer.encode_bool(6, this->supports_position);
  buffer.encode_bool(7, this->supports_tilt);
  buffer.encode_string(8, this->device_class);
  buffer.encode_bool(9, this->disabled_by_default);
  buffer.encode_string(10, this->icon);
  buffer.encode_enum<enums::EntityCategory>(11, this->entity_category);
  buffer.encode_bool(12, this->supports_stop);
  buffer.encode_uint32(13, this->device_id);
}
void ListEntitiesCoverResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_bool_field(total_size, 1, this->assumed_state, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_position, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_tilt, false);
  ProtoSize::add_string_field(total_size, 1, this->device_class, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_stop, false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool CoverStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->legacy_state = value.as_enum<enums::LegacyCoverState>();
      return true;
    }
    case 5: {
      this->current_operation = value.as_enum<enums::CoverOperation>();
      return true;
    }
    default:
      return false;
  }
}
bool CoverStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 3: {
      this->position = value.as_float();
      return true;
    }
    case 4: {
      this->tilt = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void CoverStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_enum<enums::LegacyCoverState>(2, this->legacy_state);
  buffer.encode_float(3, this->position);
  buffer.encode_float(4, this->tilt);
  buffer.encode_enum<enums::CoverOperation>(5, this->current_operation);
}
void CoverStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->legacy_state), false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->position != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->tilt != 0.0f, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->current_operation), false);
}
bool CoverCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->has_legacy_command = value.as_bool();
      return true;
    }
    case 3: {
      this->legacy_command = value.as_enum<enums::LegacyCoverCommand>();
      return true;
    }
    case 4: {
      this->has_position = value.as_bool();
      return true;
    }
    case 6: {
      this->has_tilt = value.as_bool();
      return true;
    }
    case 8: {
      this->stop = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool CoverCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 5: {
      this->position = value.as_float();
      return true;
    }
    case 7: {
      this->tilt = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void CoverCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->has_legacy_command);
  buffer.encode_enum<enums::LegacyCoverCommand>(3, this->legacy_command);
  buffer.encode_bool(4, this->has_position);
  buffer.encode_float(5, this->position);
  buffer.encode_bool(6, this->has_tilt);
  buffer.encode_float(7, this->tilt);
  buffer.encode_bool(8, this->stop);
}
void CoverCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_legacy_command, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->legacy_command), false);
  ProtoSize::add_bool_field(total_size, 1, this->has_position, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->position != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_tilt, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->tilt != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->stop, false);
}
#endif
#ifdef USE_FAN
bool ListEntitiesFanResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 5: {
      this->supports_oscillation = value.as_bool();
      return true;
    }
    case 6: {
      this->supports_speed = value.as_bool();
      return true;
    }
    case 7: {
      this->supports_direction = value.as_bool();
      return true;
    }
    case 8: {
      this->supported_speed_count = value.as_int32();
      return true;
    }
    case 9: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 11: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 13: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesFanResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 10: {
      this->icon = value.as_string();
      return true;
    }
    case 12: {
      this->supported_preset_modes.push_back(value.as_string());
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesFanResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesFanResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_bool(5, this->supports_oscillation);
  buffer.encode_bool(6, this->supports_speed);
  buffer.encode_bool(7, this->supports_direction);
  buffer.encode_int32(8, this->supported_speed_count);
  buffer.encode_bool(9, this->disabled_by_default);
  buffer.encode_string(10, this->icon);
  buffer.encode_enum<enums::EntityCategory>(11, this->entity_category);
  for (auto &it : this->supported_preset_modes) {
    buffer.encode_string(12, it, true);
  }
  buffer.encode_uint32(13, this->device_id);
}
void ListEntitiesFanResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_oscillation, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_speed, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_direction, false);
  ProtoSize::add_int32_field(total_size, 1, this->supported_speed_count, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  if (!this->supported_preset_modes.empty()) {
    for (const auto &it : this->supported_preset_modes) {
      ProtoSize::add_string_field(total_size, 1, it, true);
    }
  }
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool FanStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_bool();
      return true;
    }
    case 3: {
      this->oscillating = value.as_bool();
      return true;
    }
    case 4: {
      this->speed = value.as_enum<enums::FanSpeed>();
      return true;
    }
    case 5: {
      this->direction = value.as_enum<enums::FanDirection>();
      return true;
    }
    case 6: {
      this->speed_level = value.as_int32();
      return true;
    }
    default:
      return false;
  }
}
bool FanStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 7: {
      this->preset_mode = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool FanStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void FanStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
  buffer.encode_bool(3, this->oscillating);
  buffer.encode_enum<enums::FanSpeed>(4, this->speed);
  buffer.encode_enum<enums::FanDirection>(5, this->direction);
  buffer.encode_int32(6, this->speed_level);
  buffer.encode_string(7, this->preset_mode);
}
void FanStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->state, false);
  ProtoSize::add_bool_field(total_size, 1, this->oscillating, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->speed), false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->direction), false);
  ProtoSize::add_int32_field(total_size, 1, this->speed_level, false);
  ProtoSize::add_string_field(total_size, 1, this->preset_mode, false);
}
bool FanCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->has_state = value.as_bool();
      return true;
    }
    case 3: {
      this->state = value.as_bool();
      return true;
    }
    case 4: {
      this->has_speed = value.as_bool();
      return true;
    }
    case 5: {
      this->speed = value.as_enum<enums::FanSpeed>();
      return true;
    }
    case 6: {
      this->has_oscillating = value.as_bool();
      return true;
    }
    case 7: {
      this->oscillating = value.as_bool();
      return true;
    }
    case 8: {
      this->has_direction = value.as_bool();
      return true;
    }
    case 9: {
      this->direction = value.as_enum<enums::FanDirection>();
      return true;
    }
    case 10: {
      this->has_speed_level = value.as_bool();
      return true;
    }
    case 11: {
      this->speed_level = value.as_int32();
      return true;
    }
    case 12: {
      this->has_preset_mode = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool FanCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 13: {
      this->preset_mode = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool FanCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void FanCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->has_state);
  buffer.encode_bool(3, this->state);
  buffer.encode_bool(4, this->has_speed);
  buffer.encode_enum<enums::FanSpeed>(5, this->speed);
  buffer.encode_bool(6, this->has_oscillating);
  buffer.encode_bool(7, this->oscillating);
  buffer.encode_bool(8, this->has_direction);
  buffer.encode_enum<enums::FanDirection>(9, this->direction);
  buffer.encode_bool(10, this->has_speed_level);
  buffer.encode_int32(11, this->speed_level);
  buffer.encode_bool(12, this->has_preset_mode);
  buffer.encode_string(13, this->preset_mode);
}
void FanCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_state, false);
  ProtoSize::add_bool_field(total_size, 1, this->state, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_speed, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->speed), false);
  ProtoSize::add_bool_field(total_size, 1, this->has_oscillating, false);
  ProtoSize::add_bool_field(total_size, 1, this->oscillating, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_direction, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->direction), false);
  ProtoSize::add_bool_field(total_size, 1, this->has_speed_level, false);
  ProtoSize::add_int32_field(total_size, 1, this->speed_level, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_preset_mode, false);
  ProtoSize::add_string_field(total_size, 1, this->preset_mode, false);
}
#endif
#ifdef USE_LIGHT
bool ListEntitiesLightResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 12: {
      this->supported_color_modes.push_back(value.as_enum<enums::ColorMode>());
      return true;
    }
    case 5: {
      this->legacy_supports_brightness = value.as_bool();
      return true;
    }
    case 6: {
      this->legacy_supports_rgb = value.as_bool();
      return true;
    }
    case 7: {
      this->legacy_supports_white_value = value.as_bool();
      return true;
    }
    case 8: {
      this->legacy_supports_color_temperature = value.as_bool();
      return true;
    }
    case 13: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 15: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 16: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesLightResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 11: {
      this->effects.push_back(value.as_string());
      return true;
    }
    case 14: {
      this->icon = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesLightResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    case 9: {
      this->min_mireds = value.as_float();
      return true;
    }
    case 10: {
      this->max_mireds = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesLightResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  for (auto &it : this->supported_color_modes) {
    buffer.encode_enum<enums::ColorMode>(12, it, true);
  }
  buffer.encode_bool(5, this->legacy_supports_brightness);
  buffer.encode_bool(6, this->legacy_supports_rgb);
  buffer.encode_bool(7, this->legacy_supports_white_value);
  buffer.encode_bool(8, this->legacy_supports_color_temperature);
  buffer.encode_float(9, this->min_mireds);
  buffer.encode_float(10, this->max_mireds);
  for (auto &it : this->effects) {
    buffer.encode_string(11, it, true);
  }
  buffer.encode_bool(13, this->disabled_by_default);
  buffer.encode_string(14, this->icon);
  buffer.encode_enum<enums::EntityCategory>(15, this->entity_category);
  buffer.encode_uint32(16, this->device_id);
}
void ListEntitiesLightResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  if (!this->supported_color_modes.empty()) {
    for (const auto &it : this->supported_color_modes) {
      ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(it), true);
    }
  }
  ProtoSize::add_bool_field(total_size, 1, this->legacy_supports_brightness, false);
  ProtoSize::add_bool_field(total_size, 1, this->legacy_supports_rgb, false);
  ProtoSize::add_bool_field(total_size, 1, this->legacy_supports_white_value, false);
  ProtoSize::add_bool_field(total_size, 1, this->legacy_supports_color_temperature, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->min_mireds != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->max_mireds != 0.0f, false);
  if (!this->effects.empty()) {
    for (const auto &it : this->effects) {
      ProtoSize::add_string_field(total_size, 1, it, true);
    }
  }
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_uint32_field(total_size, 2, this->device_id, false);
}
bool LightStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_bool();
      return true;
    }
    case 11: {
      this->color_mode = value.as_enum<enums::ColorMode>();
      return true;
    }
    default:
      return false;
  }
}
bool LightStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 9: {
      this->effect = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool LightStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 3: {
      this->brightness = value.as_float();
      return true;
    }
    case 10: {
      this->color_brightness = value.as_float();
      return true;
    }
    case 4: {
      this->red = value.as_float();
      return true;
    }
    case 5: {
      this->green = value.as_float();
      return true;
    }
    case 6: {
      this->blue = value.as_float();
      return true;
    }
    case 7: {
      this->white = value.as_float();
      return true;
    }
    case 8: {
      this->color_temperature = value.as_float();
      return true;
    }
    case 12: {
      this->cold_white = value.as_float();
      return true;
    }
    case 13: {
      this->warm_white = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void LightStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
  buffer.encode_float(3, this->brightness);
  buffer.encode_enum<enums::ColorMode>(11, this->color_mode);
  buffer.encode_float(10, this->color_brightness);
  buffer.encode_float(4, this->red);
  buffer.encode_float(5, this->green);
  buffer.encode_float(6, this->blue);
  buffer.encode_float(7, this->white);
  buffer.encode_float(8, this->color_temperature);
  buffer.encode_float(12, this->cold_white);
  buffer.encode_float(13, this->warm_white);
  buffer.encode_string(9, this->effect);
}
void LightStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->state, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->brightness != 0.0f, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->color_mode), false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->color_brightness != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->red != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->green != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->blue != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->white != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->color_temperature != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->cold_white != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->warm_white != 0.0f, false);
  ProtoSize::add_string_field(total_size, 1, this->effect, false);
}
bool LightCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->has_state = value.as_bool();
      return true;
    }
    case 3: {
      this->state = value.as_bool();
      return true;
    }
    case 4: {
      this->has_brightness = value.as_bool();
      return true;
    }
    case 22: {
      this->has_color_mode = value.as_bool();
      return true;
    }
    case 23: {
      this->color_mode = value.as_enum<enums::ColorMode>();
      return true;
    }
    case 20: {
      this->has_color_brightness = value.as_bool();
      return true;
    }
    case 6: {
      this->has_rgb = value.as_bool();
      return true;
    }
    case 10: {
      this->has_white = value.as_bool();
      return true;
    }
    case 12: {
      this->has_color_temperature = value.as_bool();
      return true;
    }
    case 24: {
      this->has_cold_white = value.as_bool();
      return true;
    }
    case 26: {
      this->has_warm_white = value.as_bool();
      return true;
    }
    case 14: {
      this->has_transition_length = value.as_bool();
      return true;
    }
    case 15: {
      this->transition_length = value.as_uint32();
      return true;
    }
    case 16: {
      this->has_flash_length = value.as_bool();
      return true;
    }
    case 17: {
      this->flash_length = value.as_uint32();
      return true;
    }
    case 18: {
      this->has_effect = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool LightCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 19: {
      this->effect = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool LightCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 5: {
      this->brightness = value.as_float();
      return true;
    }
    case 21: {
      this->color_brightness = value.as_float();
      return true;
    }
    case 7: {
      this->red = value.as_float();
      return true;
    }
    case 8: {
      this->green = value.as_float();
      return true;
    }
    case 9: {
      this->blue = value.as_float();
      return true;
    }
    case 11: {
      this->white = value.as_float();
      return true;
    }
    case 13: {
      this->color_temperature = value.as_float();
      return true;
    }
    case 25: {
      this->cold_white = value.as_float();
      return true;
    }
    case 27: {
      this->warm_white = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void LightCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->has_state);
  buffer.encode_bool(3, this->state);
  buffer.encode_bool(4, this->has_brightness);
  buffer.encode_float(5, this->brightness);
  buffer.encode_bool(22, this->has_color_mode);
  buffer.encode_enum<enums::ColorMode>(23, this->color_mode);
  buffer.encode_bool(20, this->has_color_brightness);
  buffer.encode_float(21, this->color_brightness);
  buffer.encode_bool(6, this->has_rgb);
  buffer.encode_float(7, this->red);
  buffer.encode_float(8, this->green);
  buffer.encode_float(9, this->blue);
  buffer.encode_bool(10, this->has_white);
  buffer.encode_float(11, this->white);
  buffer.encode_bool(12, this->has_color_temperature);
  buffer.encode_float(13, this->color_temperature);
  buffer.encode_bool(24, this->has_cold_white);
  buffer.encode_float(25, this->cold_white);
  buffer.encode_bool(26, this->has_warm_white);
  buffer.encode_float(27, this->warm_white);
  buffer.encode_bool(14, this->has_transition_length);
  buffer.encode_uint32(15, this->transition_length);
  buffer.encode_bool(16, this->has_flash_length);
  buffer.encode_uint32(17, this->flash_length);
  buffer.encode_bool(18, this->has_effect);
  buffer.encode_string(19, this->effect);
}
void LightCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_state, false);
  ProtoSize::add_bool_field(total_size, 1, this->state, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_brightness, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->brightness != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 2, this->has_color_mode, false);
  ProtoSize::add_enum_field(total_size, 2, static_cast<uint32_t>(this->color_mode), false);
  ProtoSize::add_bool_field(total_size, 2, this->has_color_brightness, false);
  ProtoSize::add_fixed_field<4>(total_size, 2, this->color_brightness != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_rgb, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->red != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->green != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->blue != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_white, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->white != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_color_temperature, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->color_temperature != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 2, this->has_cold_white, false);
  ProtoSize::add_fixed_field<4>(total_size, 2, this->cold_white != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 2, this->has_warm_white, false);
  ProtoSize::add_fixed_field<4>(total_size, 2, this->warm_white != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_transition_length, false);
  ProtoSize::add_uint32_field(total_size, 1, this->transition_length, false);
  ProtoSize::add_bool_field(total_size, 2, this->has_flash_length, false);
  ProtoSize::add_uint32_field(total_size, 2, this->flash_length, false);
  ProtoSize::add_bool_field(total_size, 2, this->has_effect, false);
  ProtoSize::add_string_field(total_size, 2, this->effect, false);
}
#endif
#ifdef USE_SENSOR
bool ListEntitiesSensorResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 7: {
      this->accuracy_decimals = value.as_int32();
      return true;
    }
    case 8: {
      this->force_update = value.as_bool();
      return true;
    }
    case 10: {
      this->state_class = value.as_enum<enums::SensorStateClass>();
      return true;
    }
    case 11: {
      this->legacy_last_reset_type = value.as_enum<enums::SensorLastResetType>();
      return true;
    }
    case 12: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 13: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 14: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesSensorResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 6: {
      this->unit_of_measurement = value.as_string();
      return true;
    }
    case 9: {
      this->device_class = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesSensorResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesSensorResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_string(6, this->unit_of_measurement);
  buffer.encode_int32(7, this->accuracy_decimals);
  buffer.encode_bool(8, this->force_update);
  buffer.encode_string(9, this->device_class);
  buffer.encode_enum<enums::SensorStateClass>(10, this->state_class);
  buffer.encode_enum<enums::SensorLastResetType>(11, this->legacy_last_reset_type);
  buffer.encode_bool(12, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(13, this->entity_category);
  buffer.encode_uint32(14, this->device_id);
}
void ListEntitiesSensorResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_string_field(total_size, 1, this->unit_of_measurement, false);
  ProtoSize::add_int32_field(total_size, 1, this->accuracy_decimals, false);
  ProtoSize::add_bool_field(total_size, 1, this->force_update, false);
  ProtoSize::add_string_field(total_size, 1, this->device_class, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->state_class), false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->legacy_last_reset_type), false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool SensorStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 3: {
      this->missing_state = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool SensorStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 2: {
      this->state = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void SensorStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_float(2, this->state);
  buffer.encode_bool(3, this->missing_state);
}
void SensorStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->state != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->missing_state, false);
}
#endif
#ifdef USE_SWITCH
bool ListEntitiesSwitchResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->assumed_state = value.as_bool();
      return true;
    }
    case 7: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 8: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 10: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesSwitchResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 9: {
      this->device_class = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesSwitchResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesSwitchResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->assumed_state);
  buffer.encode_bool(7, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(8, this->entity_category);
  buffer.encode_string(9, this->device_class);
  buffer.encode_uint32(10, this->device_id);
}
void ListEntitiesSwitchResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->assumed_state, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_string_field(total_size, 1, this->device_class, false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool SwitchStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool SwitchStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void SwitchStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
}
void SwitchStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->state, false);
}
bool SwitchCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool SwitchCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void SwitchCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
}
void SwitchCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->state, false);
}
#endif
#ifdef USE_TEXT_SENSOR
bool ListEntitiesTextSensorResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 9: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesTextSensorResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 8: {
      this->device_class = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesTextSensorResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesTextSensorResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_string(8, this->device_class);
  buffer.encode_uint32(9, this->device_id);
}
void ListEntitiesTextSensorResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_string_field(total_size, 1, this->device_class, false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool TextSensorStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 3: {
      this->missing_state = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool TextSensorStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool TextSensorStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void TextSensorStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_string(2, this->state);
  buffer.encode_bool(3, this->missing_state);
}
void TextSensorStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->state, false);
  ProtoSize::add_bool_field(total_size, 1, this->missing_state, false);
}
#endif
bool SubscribeLogsRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->level = value.as_enum<enums::LogLevel>();
      return true;
    }
    case 2: {
      this->dump_config = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
void SubscribeLogsRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_enum<enums::LogLevel>(1, this->level);
  buffer.encode_bool(2, this->dump_config);
}
void SubscribeLogsRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->level), false);
  ProtoSize::add_bool_field(total_size, 1, this->dump_config, false);
}
bool SubscribeLogsResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->level = value.as_enum<enums::LogLevel>();
      return true;
    }
    case 4: {
      this->send_failed = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool SubscribeLogsResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3: {
      this->message = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void SubscribeLogsResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_enum<enums::LogLevel>(1, this->level);
  buffer.encode_bytes(3, reinterpret_cast<const uint8_t *>(this->message.data()), this->message.size());
  buffer.encode_bool(4, this->send_failed);
}
void SubscribeLogsResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->level), false);
  ProtoSize::add_string_field(total_size, 1, this->message, false);
  ProtoSize::add_bool_field(total_size, 1, this->send_failed, false);
}
#ifdef USE_API_NOISE
bool NoiseEncryptionSetKeyRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void NoiseEncryptionSetKeyRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_bytes(1, reinterpret_cast<const uint8_t *>(this->key.data()), this->key.size());
}
void NoiseEncryptionSetKeyRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->key, false);
}
bool NoiseEncryptionSetKeyResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->success = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
void NoiseEncryptionSetKeyResponse::encode(ProtoWriteBuffer buffer) const { buffer.encode_bool(1, this->success); }
void NoiseEncryptionSetKeyResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_bool_field(total_size, 1, this->success, false);
}
#endif
bool HomeassistantServiceMap::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_string();
      return true;
    }
    case 2: {
      this->value = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void HomeassistantServiceMap::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->key);
  buffer.encode_string(2, this->value);
}
void HomeassistantServiceMap::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->key, false);
  ProtoSize::add_string_field(total_size, 1, this->value, false);
}
bool HomeassistantServiceResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 5: {
      this->is_event = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool HomeassistantServiceResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->service = value.as_string();
      return true;
    }
    case 2: {
      this->data.push_back(value.as_message<HomeassistantServiceMap>());
      return true;
    }
    case 3: {
      this->data_template.push_back(value.as_message<HomeassistantServiceMap>());
      return true;
    }
    case 4: {
      this->variables.push_back(value.as_message<HomeassistantServiceMap>());
      return true;
    }
    default:
      return false;
  }
}
void HomeassistantServiceResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->service);
  for (auto &it : this->data) {
    buffer.encode_message<HomeassistantServiceMap>(2, it, true);
  }
  for (auto &it : this->data_template) {
    buffer.encode_message<HomeassistantServiceMap>(3, it, true);
  }
  for (auto &it : this->variables) {
    buffer.encode_message<HomeassistantServiceMap>(4, it, true);
  }
  buffer.encode_bool(5, this->is_event);
}
void HomeassistantServiceResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->service, false);
  ProtoSize::add_repeated_message(total_size, 1, this->data);
  ProtoSize::add_repeated_message(total_size, 1, this->data_template);
  ProtoSize::add_repeated_message(total_size, 1, this->variables);
  ProtoSize::add_bool_field(total_size, 1, this->is_event, false);
}
bool SubscribeHomeAssistantStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 3: {
      this->once = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool SubscribeHomeAssistantStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->entity_id = value.as_string();
      return true;
    }
    case 2: {
      this->attribute = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void SubscribeHomeAssistantStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->entity_id);
  buffer.encode_string(2, this->attribute);
  buffer.encode_bool(3, this->once);
}
void SubscribeHomeAssistantStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->entity_id, false);
  ProtoSize::add_string_field(total_size, 1, this->attribute, false);
  ProtoSize::add_bool_field(total_size, 1, this->once, false);
}
bool HomeAssistantStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->entity_id = value.as_string();
      return true;
    }
    case 2: {
      this->state = value.as_string();
      return true;
    }
    case 3: {
      this->attribute = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void HomeAssistantStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->entity_id);
  buffer.encode_string(2, this->state);
  buffer.encode_string(3, this->attribute);
}
void HomeAssistantStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->entity_id, false);
  ProtoSize::add_string_field(total_size, 1, this->state, false);
  ProtoSize::add_string_field(total_size, 1, this->attribute, false);
}
bool GetTimeResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->epoch_seconds = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void GetTimeResponse::encode(ProtoWriteBuffer buffer) const { buffer.encode_fixed32(1, this->epoch_seconds); }
void GetTimeResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->epoch_seconds != 0, false);
}
bool ListEntitiesServicesArgument::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->type = value.as_enum<enums::ServiceArgType>();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesServicesArgument::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->name = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesServicesArgument::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->name);
  buffer.encode_enum<enums::ServiceArgType>(2, this->type);
}
void ListEntitiesServicesArgument::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->type), false);
}
bool ListEntitiesServicesResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->name = value.as_string();
      return true;
    }
    case 3: {
      this->args.push_back(value.as_message<ListEntitiesServicesArgument>());
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesServicesResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesServicesResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->name);
  buffer.encode_fixed32(2, this->key);
  for (auto &it : this->args) {
    buffer.encode_message<ListEntitiesServicesArgument>(3, it, true);
  }
}
void ListEntitiesServicesResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_repeated_message(total_size, 1, this->args);
}
bool ExecuteServiceArgument::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->bool_ = value.as_bool();
      return true;
    }
    case 2: {
      this->legacy_int = value.as_int32();
      return true;
    }
    case 5: {
      this->int_ = value.as_sint32();
      return true;
    }
    case 6: {
      this->bool_array.push_back(value.as_bool());
      return true;
    }
    case 7: {
      this->int_array.push_back(value.as_sint32());
      return true;
    }
    default:
      return false;
  }
}
bool ExecuteServiceArgument::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 4: {
      this->string_ = value.as_string();
      return true;
    }
    case 9: {
      this->string_array.push_back(value.as_string());
      return true;
    }
    default:
      return false;
  }
}
bool ExecuteServiceArgument::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 3: {
      this->float_ = value.as_float();
      return true;
    }
    case 8: {
      this->float_array.push_back(value.as_float());
      return true;
    }
    default:
      return false;
  }
}
void ExecuteServiceArgument::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_bool(1, this->bool_);
  buffer.encode_int32(2, this->legacy_int);
  buffer.encode_float(3, this->float_);
  buffer.encode_string(4, this->string_);
  buffer.encode_sint32(5, this->int_);
  for (auto it : this->bool_array) {
    buffer.encode_bool(6, it, true);
  }
  for (auto &it : this->int_array) {
    buffer.encode_sint32(7, it, true);
  }
  for (auto &it : this->float_array) {
    buffer.encode_float(8, it, true);
  }
  for (auto &it : this->string_array) {
    buffer.encode_string(9, it, true);
  }
}
void ExecuteServiceArgument::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_bool_field(total_size, 1, this->bool_, false);
  ProtoSize::add_int32_field(total_size, 1, this->legacy_int, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->float_ != 0.0f, false);
  ProtoSize::add_string_field(total_size, 1, this->string_, false);
  ProtoSize::add_sint32_field(total_size, 1, this->int_, false);
  if (!this->bool_array.empty()) {
    for (const auto it : this->bool_array) {
      ProtoSize::add_bool_field(total_size, 1, it, true);
    }
  }
  if (!this->int_array.empty()) {
    for (const auto &it : this->int_array) {
      ProtoSize::add_sint32_field(total_size, 1, it, true);
    }
  }
  if (!this->float_array.empty()) {
    for (const auto &it : this->float_array) {
      ProtoSize::add_fixed_field<4>(total_size, 1, it != 0.0f, true);
    }
  }
  if (!this->string_array.empty()) {
    for (const auto &it : this->string_array) {
      ProtoSize::add_string_field(total_size, 1, it, true);
    }
  }
}
bool ExecuteServiceRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->args.push_back(value.as_message<ExecuteServiceArgument>());
      return true;
    }
    default:
      return false;
  }
}
bool ExecuteServiceRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ExecuteServiceRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  for (auto &it : this->args) {
    buffer.encode_message<ExecuteServiceArgument>(2, it, true);
  }
}
void ExecuteServiceRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_repeated_message(total_size, 1, this->args);
}
#ifdef USE_ESP32_CAMERA
bool ListEntitiesCameraResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 5: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 8: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesCameraResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 6: {
      this->icon = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesCameraResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesCameraResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_bool(5, this->disabled_by_default);
  buffer.encode_string(6, this->icon);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_uint32(8, this->device_id);
}
void ListEntitiesCameraResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool CameraImageResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 3: {
      this->done = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool CameraImageResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->data = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool CameraImageResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void CameraImageResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bytes(2, reinterpret_cast<const uint8_t *>(this->data.data()), this->data.size());
  buffer.encode_bool(3, this->done);
}
void CameraImageResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->data, false);
  ProtoSize::add_bool_field(total_size, 1, this->done, false);
}
bool CameraImageRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->single = value.as_bool();
      return true;
    }
    case 2: {
      this->stream = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
void CameraImageRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_bool(1, this->single);
  buffer.encode_bool(2, this->stream);
}
void CameraImageRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_bool_field(total_size, 1, this->single, false);
  ProtoSize::add_bool_field(total_size, 1, this->stream, false);
}
#endif
#ifdef USE_CLIMATE
bool ListEntitiesClimateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 5: {
      this->supports_current_temperature = value.as_bool();
      return true;
    }
    case 6: {
      this->supports_two_point_target_temperature = value.as_bool();
      return true;
    }
    case 7: {
      this->supported_modes.push_back(value.as_enum<enums::ClimateMode>());
      return true;
    }
    case 11: {
      this->legacy_supports_away = value.as_bool();
      return true;
    }
    case 12: {
      this->supports_action = value.as_bool();
      return true;
    }
    case 13: {
      this->supported_fan_modes.push_back(value.as_enum<enums::ClimateFanMode>());
      return true;
    }
    case 14: {
      this->supported_swing_modes.push_back(value.as_enum<enums::ClimateSwingMode>());
      return true;
    }
    case 16: {
      this->supported_presets.push_back(value.as_enum<enums::ClimatePreset>());
      return true;
    }
    case 18: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 20: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 22: {
      this->supports_current_humidity = value.as_bool();
      return true;
    }
    case 23: {
      this->supports_target_humidity = value.as_bool();
      return true;
    }
    case 26: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesClimateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 15: {
      this->supported_custom_fan_modes.push_back(value.as_string());
      return true;
    }
    case 17: {
      this->supported_custom_presets.push_back(value.as_string());
      return true;
    }
    case 19: {
      this->icon = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesClimateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    case 8: {
      this->visual_min_temperature = value.as_float();
      return true;
    }
    case 9: {
      this->visual_max_temperature = value.as_float();
      return true;
    }
    case 10: {
      this->visual_target_temperature_step = value.as_float();
      return true;
    }
    case 21: {
      this->visual_current_temperature_step = value.as_float();
      return true;
    }
    case 24: {
      this->visual_min_humidity = value.as_float();
      return true;
    }
    case 25: {
      this->visual_max_humidity = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesClimateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_bool(5, this->supports_current_temperature);
  buffer.encode_bool(6, this->supports_two_point_target_temperature);
  for (auto &it : this->supported_modes) {
    buffer.encode_enum<enums::ClimateMode>(7, it, true);
  }
  buffer.encode_float(8, this->visual_min_temperature);
  buffer.encode_float(9, this->visual_max_temperature);
  buffer.encode_float(10, this->visual_target_temperature_step);
  buffer.encode_bool(11, this->legacy_supports_away);
  buffer.encode_bool(12, this->supports_action);
  for (auto &it : this->supported_fan_modes) {
    buffer.encode_enum<enums::ClimateFanMode>(13, it, true);
  }
  for (auto &it : this->supported_swing_modes) {
    buffer.encode_enum<enums::ClimateSwingMode>(14, it, true);
  }
  for (auto &it : this->supported_custom_fan_modes) {
    buffer.encode_string(15, it, true);
  }
  for (auto &it : this->supported_presets) {
    buffer.encode_enum<enums::ClimatePreset>(16, it, true);
  }
  for (auto &it : this->supported_custom_presets) {
    buffer.encode_string(17, it, true);
  }
  buffer.encode_bool(18, this->disabled_by_default);
  buffer.encode_string(19, this->icon);
  buffer.encode_enum<enums::EntityCategory>(20, this->entity_category);
  buffer.encode_float(21, this->visual_current_temperature_step);
  buffer.encode_bool(22, this->supports_current_humidity);
  buffer.encode_bool(23, this->supports_target_humidity);
  buffer.encode_float(24, this->visual_min_humidity);
  buffer.encode_float(25, this->visual_max_humidity);
  buffer.encode_uint32(26, this->device_id);
}
void ListEntitiesClimateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_current_temperature, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_two_point_target_temperature, false);
  if (!this->supported_modes.empty()) {
    for (const auto &it : this->supported_modes) {
      ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(it), true);
    }
  }
  ProtoSize::add_fixed_field<4>(total_size, 1, this->visual_min_temperature != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->visual_max_temperature != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->visual_target_temperature_step != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->legacy_supports_away, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_action, false);
  if (!this->supported_fan_modes.empty()) {
    for (const auto &it : this->supported_fan_modes) {
      ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(it), true);
    }
  }
  if (!this->supported_swing_modes.empty()) {
    for (const auto &it : this->supported_swing_modes) {
      ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(it), true);
    }
  }
  if (!this->supported_custom_fan_modes.empty()) {
    for (const auto &it : this->supported_custom_fan_modes) {
      ProtoSize::add_string_field(total_size, 1, it, true);
    }
  }
  if (!this->supported_presets.empty()) {
    for (const auto &it : this->supported_presets) {
      ProtoSize::add_enum_field(total_size, 2, static_cast<uint32_t>(it), true);
    }
  }
  if (!this->supported_custom_presets.empty()) {
    for (const auto &it : this->supported_custom_presets) {
      ProtoSize::add_string_field(total_size, 2, it, true);
    }
  }
  ProtoSize::add_bool_field(total_size, 2, this->disabled_by_default, false);
  ProtoSize::add_string_field(total_size, 2, this->icon, false);
  ProtoSize::add_enum_field(total_size, 2, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_fixed_field<4>(total_size, 2, this->visual_current_temperature_step != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 2, this->supports_current_humidity, false);
  ProtoSize::add_bool_field(total_size, 2, this->supports_target_humidity, false);
  ProtoSize::add_fixed_field<4>(total_size, 2, this->visual_min_humidity != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 2, this->visual_max_humidity != 0.0f, false);
  ProtoSize::add_uint32_field(total_size, 2, this->device_id, false);
}
bool ClimateStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->mode = value.as_enum<enums::ClimateMode>();
      return true;
    }
    case 7: {
      this->unused_legacy_away = value.as_bool();
      return true;
    }
    case 8: {
      this->action = value.as_enum<enums::ClimateAction>();
      return true;
    }
    case 9: {
      this->fan_mode = value.as_enum<enums::ClimateFanMode>();
      return true;
    }
    case 10: {
      this->swing_mode = value.as_enum<enums::ClimateSwingMode>();
      return true;
    }
    case 12: {
      this->preset = value.as_enum<enums::ClimatePreset>();
      return true;
    }
    default:
      return false;
  }
}
bool ClimateStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 11: {
      this->custom_fan_mode = value.as_string();
      return true;
    }
    case 13: {
      this->custom_preset = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ClimateStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 3: {
      this->current_temperature = value.as_float();
      return true;
    }
    case 4: {
      this->target_temperature = value.as_float();
      return true;
    }
    case 5: {
      this->target_temperature_low = value.as_float();
      return true;
    }
    case 6: {
      this->target_temperature_high = value.as_float();
      return true;
    }
    case 14: {
      this->current_humidity = value.as_float();
      return true;
    }
    case 15: {
      this->target_humidity = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void ClimateStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_enum<enums::ClimateMode>(2, this->mode);
  buffer.encode_float(3, this->current_temperature);
  buffer.encode_float(4, this->target_temperature);
  buffer.encode_float(5, this->target_temperature_low);
  buffer.encode_float(6, this->target_temperature_high);
  buffer.encode_bool(7, this->unused_legacy_away);
  buffer.encode_enum<enums::ClimateAction>(8, this->action);
  buffer.encode_enum<enums::ClimateFanMode>(9, this->fan_mode);
  buffer.encode_enum<enums::ClimateSwingMode>(10, this->swing_mode);
  buffer.encode_string(11, this->custom_fan_mode);
  buffer.encode_enum<enums::ClimatePreset>(12, this->preset);
  buffer.encode_string(13, this->custom_preset);
  buffer.encode_float(14, this->current_humidity);
  buffer.encode_float(15, this->target_humidity);
}
void ClimateStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->mode), false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->current_temperature != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->target_temperature != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->target_temperature_low != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->target_temperature_high != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->unused_legacy_away, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->action), false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->fan_mode), false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->swing_mode), false);
  ProtoSize::add_string_field(total_size, 1, this->custom_fan_mode, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->preset), false);
  ProtoSize::add_string_field(total_size, 1, this->custom_preset, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->current_humidity != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->target_humidity != 0.0f, false);
}
bool ClimateCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->has_mode = value.as_bool();
      return true;
    }
    case 3: {
      this->mode = value.as_enum<enums::ClimateMode>();
      return true;
    }
    case 4: {
      this->has_target_temperature = value.as_bool();
      return true;
    }
    case 6: {
      this->has_target_temperature_low = value.as_bool();
      return true;
    }
    case 8: {
      this->has_target_temperature_high = value.as_bool();
      return true;
    }
    case 10: {
      this->unused_has_legacy_away = value.as_bool();
      return true;
    }
    case 11: {
      this->unused_legacy_away = value.as_bool();
      return true;
    }
    case 12: {
      this->has_fan_mode = value.as_bool();
      return true;
    }
    case 13: {
      this->fan_mode = value.as_enum<enums::ClimateFanMode>();
      return true;
    }
    case 14: {
      this->has_swing_mode = value.as_bool();
      return true;
    }
    case 15: {
      this->swing_mode = value.as_enum<enums::ClimateSwingMode>();
      return true;
    }
    case 16: {
      this->has_custom_fan_mode = value.as_bool();
      return true;
    }
    case 18: {
      this->has_preset = value.as_bool();
      return true;
    }
    case 19: {
      this->preset = value.as_enum<enums::ClimatePreset>();
      return true;
    }
    case 20: {
      this->has_custom_preset = value.as_bool();
      return true;
    }
    case 22: {
      this->has_target_humidity = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool ClimateCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 17: {
      this->custom_fan_mode = value.as_string();
      return true;
    }
    case 21: {
      this->custom_preset = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ClimateCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 5: {
      this->target_temperature = value.as_float();
      return true;
    }
    case 7: {
      this->target_temperature_low = value.as_float();
      return true;
    }
    case 9: {
      this->target_temperature_high = value.as_float();
      return true;
    }
    case 23: {
      this->target_humidity = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void ClimateCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->has_mode);
  buffer.encode_enum<enums::ClimateMode>(3, this->mode);
  buffer.encode_bool(4, this->has_target_temperature);
  buffer.encode_float(5, this->target_temperature);
  buffer.encode_bool(6, this->has_target_temperature_low);
  buffer.encode_float(7, this->target_temperature_low);
  buffer.encode_bool(8, this->has_target_temperature_high);
  buffer.encode_float(9, this->target_temperature_high);
  buffer.encode_bool(10, this->unused_has_legacy_away);
  buffer.encode_bool(11, this->unused_legacy_away);
  buffer.encode_bool(12, this->has_fan_mode);
  buffer.encode_enum<enums::ClimateFanMode>(13, this->fan_mode);
  buffer.encode_bool(14, this->has_swing_mode);
  buffer.encode_enum<enums::ClimateSwingMode>(15, this->swing_mode);
  buffer.encode_bool(16, this->has_custom_fan_mode);
  buffer.encode_string(17, this->custom_fan_mode);
  buffer.encode_bool(18, this->has_preset);
  buffer.encode_enum<enums::ClimatePreset>(19, this->preset);
  buffer.encode_bool(20, this->has_custom_preset);
  buffer.encode_string(21, this->custom_preset);
  buffer.encode_bool(22, this->has_target_humidity);
  buffer.encode_float(23, this->target_humidity);
}
void ClimateCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_mode, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->mode), false);
  ProtoSize::add_bool_field(total_size, 1, this->has_target_temperature, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->target_temperature != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_target_temperature_low, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->target_temperature_low != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_target_temperature_high, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->target_temperature_high != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->unused_has_legacy_away, false);
  ProtoSize::add_bool_field(total_size, 1, this->unused_legacy_away, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_fan_mode, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->fan_mode), false);
  ProtoSize::add_bool_field(total_size, 1, this->has_swing_mode, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->swing_mode), false);
  ProtoSize::add_bool_field(total_size, 2, this->has_custom_fan_mode, false);
  ProtoSize::add_string_field(total_size, 2, this->custom_fan_mode, false);
  ProtoSize::add_bool_field(total_size, 2, this->has_preset, false);
  ProtoSize::add_enum_field(total_size, 2, static_cast<uint32_t>(this->preset), false);
  ProtoSize::add_bool_field(total_size, 2, this->has_custom_preset, false);
  ProtoSize::add_string_field(total_size, 2, this->custom_preset, false);
  ProtoSize::add_bool_field(total_size, 2, this->has_target_humidity, false);
  ProtoSize::add_fixed_field<4>(total_size, 2, this->target_humidity != 0.0f, false);
}
#endif
#ifdef USE_NUMBER
bool ListEntitiesNumberResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 9: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 10: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 12: {
      this->mode = value.as_enum<enums::NumberMode>();
      return true;
    }
    case 14: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesNumberResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 11: {
      this->unit_of_measurement = value.as_string();
      return true;
    }
    case 13: {
      this->device_class = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesNumberResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    case 6: {
      this->min_value = value.as_float();
      return true;
    }
    case 7: {
      this->max_value = value.as_float();
      return true;
    }
    case 8: {
      this->step = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesNumberResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_float(6, this->min_value);
  buffer.encode_float(7, this->max_value);
  buffer.encode_float(8, this->step);
  buffer.encode_bool(9, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(10, this->entity_category);
  buffer.encode_string(11, this->unit_of_measurement);
  buffer.encode_enum<enums::NumberMode>(12, this->mode);
  buffer.encode_string(13, this->device_class);
  buffer.encode_uint32(14, this->device_id);
}
void ListEntitiesNumberResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->min_value != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->max_value != 0.0f, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->step != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_string_field(total_size, 1, this->unit_of_measurement, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->mode), false);
  ProtoSize::add_string_field(total_size, 1, this->device_class, false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool NumberStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 3: {
      this->missing_state = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool NumberStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 2: {
      this->state = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void NumberStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_float(2, this->state);
  buffer.encode_bool(3, this->missing_state);
}
void NumberStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->state != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->missing_state, false);
}
bool NumberCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 2: {
      this->state = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void NumberCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_float(2, this->state);
}
void NumberCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->state != 0.0f, false);
}
#endif
#ifdef USE_SELECT
bool ListEntitiesSelectResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 7: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 8: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 9: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesSelectResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 6: {
      this->options.push_back(value.as_string());
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesSelectResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesSelectResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  for (auto &it : this->options) {
    buffer.encode_string(6, it, true);
  }
  buffer.encode_bool(7, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(8, this->entity_category);
  buffer.encode_uint32(9, this->device_id);
}
void ListEntitiesSelectResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  if (!this->options.empty()) {
    for (const auto &it : this->options) {
      ProtoSize::add_string_field(total_size, 1, it, true);
    }
  }
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool SelectStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 3: {
      this->missing_state = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool SelectStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool SelectStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void SelectStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_string(2, this->state);
  buffer.encode_bool(3, this->missing_state);
}
void SelectStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->state, false);
  ProtoSize::add_bool_field(total_size, 1, this->missing_state, false);
}
bool SelectCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool SelectCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void SelectCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_string(2, this->state);
}
void SelectCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->state, false);
}
#endif
#ifdef USE_SIREN
bool ListEntitiesSirenResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 8: {
      this->supports_duration = value.as_bool();
      return true;
    }
    case 9: {
      this->supports_volume = value.as_bool();
      return true;
    }
    case 10: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 11: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesSirenResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 7: {
      this->tones.push_back(value.as_string());
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesSirenResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesSirenResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  for (auto &it : this->tones) {
    buffer.encode_string(7, it, true);
  }
  buffer.encode_bool(8, this->supports_duration);
  buffer.encode_bool(9, this->supports_volume);
  buffer.encode_enum<enums::EntityCategory>(10, this->entity_category);
  buffer.encode_uint32(11, this->device_id);
}
void ListEntitiesSirenResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  if (!this->tones.empty()) {
    for (const auto &it : this->tones) {
      ProtoSize::add_string_field(total_size, 1, it, true);
    }
  }
  ProtoSize::add_bool_field(total_size, 1, this->supports_duration, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_volume, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool SirenStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool SirenStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void SirenStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
}
void SirenStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->state, false);
}
bool SirenCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->has_state = value.as_bool();
      return true;
    }
    case 3: {
      this->state = value.as_bool();
      return true;
    }
    case 4: {
      this->has_tone = value.as_bool();
      return true;
    }
    case 6: {
      this->has_duration = value.as_bool();
      return true;
    }
    case 7: {
      this->duration = value.as_uint32();
      return true;
    }
    case 8: {
      this->has_volume = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool SirenCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 5: {
      this->tone = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool SirenCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 9: {
      this->volume = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void SirenCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->has_state);
  buffer.encode_bool(3, this->state);
  buffer.encode_bool(4, this->has_tone);
  buffer.encode_string(5, this->tone);
  buffer.encode_bool(6, this->has_duration);
  buffer.encode_uint32(7, this->duration);
  buffer.encode_bool(8, this->has_volume);
  buffer.encode_float(9, this->volume);
}
void SirenCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_state, false);
  ProtoSize::add_bool_field(total_size, 1, this->state, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_tone, false);
  ProtoSize::add_string_field(total_size, 1, this->tone, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_duration, false);
  ProtoSize::add_uint32_field(total_size, 1, this->duration, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_volume, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->volume != 0.0f, false);
}
#endif
#ifdef USE_LOCK
bool ListEntitiesLockResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 8: {
      this->assumed_state = value.as_bool();
      return true;
    }
    case 9: {
      this->supports_open = value.as_bool();
      return true;
    }
    case 10: {
      this->requires_code = value.as_bool();
      return true;
    }
    case 12: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesLockResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 11: {
      this->code_format = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesLockResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesLockResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_bool(8, this->assumed_state);
  buffer.encode_bool(9, this->supports_open);
  buffer.encode_bool(10, this->requires_code);
  buffer.encode_string(11, this->code_format);
  buffer.encode_uint32(12, this->device_id);
}
void ListEntitiesLockResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_bool_field(total_size, 1, this->assumed_state, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_open, false);
  ProtoSize::add_bool_field(total_size, 1, this->requires_code, false);
  ProtoSize::add_string_field(total_size, 1, this->code_format, false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool LockStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_enum<enums::LockState>();
      return true;
    }
    default:
      return false;
  }
}
bool LockStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void LockStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_enum<enums::LockState>(2, this->state);
}
void LockStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->state), false);
}
bool LockCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->command = value.as_enum<enums::LockCommand>();
      return true;
    }
    case 3: {
      this->has_code = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool LockCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 4: {
      this->code = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool LockCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void LockCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_enum<enums::LockCommand>(2, this->command);
  buffer.encode_bool(3, this->has_code);
  buffer.encode_string(4, this->code);
}
void LockCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->command), false);
  ProtoSize::add_bool_field(total_size, 1, this->has_code, false);
  ProtoSize::add_string_field(total_size, 1, this->code, false);
}
#endif
#ifdef USE_BUTTON
bool ListEntitiesButtonResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 9: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesButtonResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 8: {
      this->device_class = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesButtonResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesButtonResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_string(8, this->device_class);
  buffer.encode_uint32(9, this->device_id);
}
void ListEntitiesButtonResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_string_field(total_size, 1, this->device_class, false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool ButtonCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ButtonCommandRequest::encode(ProtoWriteBuffer buffer) const { buffer.encode_fixed32(1, this->key); }
void ButtonCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
}
#endif
#ifdef USE_MEDIA_PLAYER
bool MediaPlayerSupportedFormat::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->sample_rate = value.as_uint32();
      return true;
    }
    case 3: {
      this->num_channels = value.as_uint32();
      return true;
    }
    case 4: {
      this->purpose = value.as_enum<enums::MediaPlayerFormatPurpose>();
      return true;
    }
    case 5: {
      this->sample_bytes = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool MediaPlayerSupportedFormat::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->format = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void MediaPlayerSupportedFormat::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->format);
  buffer.encode_uint32(2, this->sample_rate);
  buffer.encode_uint32(3, this->num_channels);
  buffer.encode_enum<enums::MediaPlayerFormatPurpose>(4, this->purpose);
  buffer.encode_uint32(5, this->sample_bytes);
}
void MediaPlayerSupportedFormat::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->format, false);
  ProtoSize::add_uint32_field(total_size, 1, this->sample_rate, false);
  ProtoSize::add_uint32_field(total_size, 1, this->num_channels, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->purpose), false);
  ProtoSize::add_uint32_field(total_size, 1, this->sample_bytes, false);
}
bool ListEntitiesMediaPlayerResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 8: {
      this->supports_pause = value.as_bool();
      return true;
    }
    case 10: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesMediaPlayerResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 9: {
      this->supported_formats.push_back(value.as_message<MediaPlayerSupportedFormat>());
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesMediaPlayerResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesMediaPlayerResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_bool(8, this->supports_pause);
  for (auto &it : this->supported_formats) {
    buffer.encode_message<MediaPlayerSupportedFormat>(9, it, true);
  }
  buffer.encode_uint32(10, this->device_id);
}
void ListEntitiesMediaPlayerResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_pause, false);
  ProtoSize::add_repeated_message(total_size, 1, this->supported_formats);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool MediaPlayerStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_enum<enums::MediaPlayerState>();
      return true;
    }
    case 4: {
      this->muted = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool MediaPlayerStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 3: {
      this->volume = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void MediaPlayerStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_enum<enums::MediaPlayerState>(2, this->state);
  buffer.encode_float(3, this->volume);
  buffer.encode_bool(4, this->muted);
}
void MediaPlayerStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->state), false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->volume != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->muted, false);
}
bool MediaPlayerCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->has_command = value.as_bool();
      return true;
    }
    case 3: {
      this->command = value.as_enum<enums::MediaPlayerCommand>();
      return true;
    }
    case 4: {
      this->has_volume = value.as_bool();
      return true;
    }
    case 6: {
      this->has_media_url = value.as_bool();
      return true;
    }
    case 8: {
      this->has_announcement = value.as_bool();
      return true;
    }
    case 9: {
      this->announcement = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool MediaPlayerCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 7: {
      this->media_url = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool MediaPlayerCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 5: {
      this->volume = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void MediaPlayerCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->has_command);
  buffer.encode_enum<enums::MediaPlayerCommand>(3, this->command);
  buffer.encode_bool(4, this->has_volume);
  buffer.encode_float(5, this->volume);
  buffer.encode_bool(6, this->has_media_url);
  buffer.encode_string(7, this->media_url);
  buffer.encode_bool(8, this->has_announcement);
  buffer.encode_bool(9, this->announcement);
}
void MediaPlayerCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_command, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->command), false);
  ProtoSize::add_bool_field(total_size, 1, this->has_volume, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->volume != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_media_url, false);
  ProtoSize::add_string_field(total_size, 1, this->media_url, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_announcement, false);
  ProtoSize::add_bool_field(total_size, 1, this->announcement, false);
}
#endif
#ifdef USE_BLUETOOTH_PROXY
bool SubscribeBluetoothLEAdvertisementsRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->flags = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
void SubscribeBluetoothLEAdvertisementsRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->flags);
}
void SubscribeBluetoothLEAdvertisementsRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint32_field(total_size, 1, this->flags, false);
}
bool BluetoothServiceData::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->legacy_data.push_back(value.as_uint32());
      return true;
    }
    default:
      return false;
  }
}
bool BluetoothServiceData::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->uuid = value.as_string();
      return true;
    }
    case 3: {
      this->data = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothServiceData::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->uuid);
  for (auto &it : this->legacy_data) {
    buffer.encode_uint32(2, it, true);
  }
  buffer.encode_bytes(3, reinterpret_cast<const uint8_t *>(this->data.data()), this->data.size());
}
void BluetoothServiceData::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->uuid, false);
  if (!this->legacy_data.empty()) {
    for (const auto &it : this->legacy_data) {
      ProtoSize::add_uint32_field(total_size, 1, it, true);
    }
  }
  ProtoSize::add_string_field(total_size, 1, this->data, false);
}
bool BluetoothLEAdvertisementResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 3: {
      this->rssi = value.as_sint32();
      return true;
    }
    case 7: {
      this->address_type = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool BluetoothLEAdvertisementResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->service_uuids.push_back(value.as_string());
      return true;
    }
    case 5: {
      this->service_data.push_back(value.as_message<BluetoothServiceData>());
      return true;
    }
    case 6: {
      this->manufacturer_data.push_back(value.as_message<BluetoothServiceData>());
      return true;
    }
    default:
      return false;
  }
}
void BluetoothLEAdvertisementResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_bytes(2, reinterpret_cast<const uint8_t *>(this->name.data()), this->name.size());
  buffer.encode_sint32(3, this->rssi);
  for (auto &it : this->service_uuids) {
    buffer.encode_string(4, it, true);
  }
  for (auto &it : this->service_data) {
    buffer.encode_message<BluetoothServiceData>(5, it, true);
  }
  for (auto &it : this->manufacturer_data) {
    buffer.encode_message<BluetoothServiceData>(6, it, true);
  }
  buffer.encode_uint32(7, this->address_type);
}
void BluetoothLEAdvertisementResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_sint32_field(total_size, 1, this->rssi, false);
  if (!this->service_uuids.empty()) {
    for (const auto &it : this->service_uuids) {
      ProtoSize::add_string_field(total_size, 1, it, true);
    }
  }
  ProtoSize::add_repeated_message(total_size, 1, this->service_data);
  ProtoSize::add_repeated_message(total_size, 1, this->manufacturer_data);
  ProtoSize::add_uint32_field(total_size, 1, this->address_type, false);
}
bool BluetoothLERawAdvertisement::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->rssi = value.as_sint32();
      return true;
    }
    case 3: {
      this->address_type = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool BluetoothLERawAdvertisement::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 4: {
      this->data = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothLERawAdvertisement::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_sint32(2, this->rssi);
  buffer.encode_uint32(3, this->address_type);
  buffer.encode_bytes(4, reinterpret_cast<const uint8_t *>(this->data.data()), this->data.size());
}
void BluetoothLERawAdvertisement::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_sint32_field(total_size, 1, this->rssi, false);
  ProtoSize::add_uint32_field(total_size, 1, this->address_type, false);
  ProtoSize::add_string_field(total_size, 1, this->data, false);
}
bool BluetoothLERawAdvertisementsResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->advertisements.push_back(value.as_message<BluetoothLERawAdvertisement>());
      return true;
    }
    default:
      return false;
  }
}
void BluetoothLERawAdvertisementsResponse::encode(ProtoWriteBuffer buffer) const {
  for (auto &it : this->advertisements) {
    buffer.encode_message<BluetoothLERawAdvertisement>(1, it, true);
  }
}
void BluetoothLERawAdvertisementsResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_repeated_message(total_size, 1, this->advertisements);
}
bool BluetoothDeviceRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->request_type = value.as_enum<enums::BluetoothDeviceRequestType>();
      return true;
    }
    case 3: {
      this->has_address_type = value.as_bool();
      return true;
    }
    case 4: {
      this->address_type = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothDeviceRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_enum<enums::BluetoothDeviceRequestType>(2, this->request_type);
  buffer.encode_bool(3, this->has_address_type);
  buffer.encode_uint32(4, this->address_type);
}
void BluetoothDeviceRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->request_type), false);
  ProtoSize::add_bool_field(total_size, 1, this->has_address_type, false);
  ProtoSize::add_uint32_field(total_size, 1, this->address_type, false);
}
bool BluetoothDeviceConnectionResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->connected = value.as_bool();
      return true;
    }
    case 3: {
      this->mtu = value.as_uint32();
      return true;
    }
    case 4: {
      this->error = value.as_int32();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothDeviceConnectionResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_bool(2, this->connected);
  buffer.encode_uint32(3, this->mtu);
  buffer.encode_int32(4, this->error);
}
void BluetoothDeviceConnectionResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_bool_field(total_size, 1, this->connected, false);
  ProtoSize::add_uint32_field(total_size, 1, this->mtu, false);
  ProtoSize::add_int32_field(total_size, 1, this->error, false);
}
bool BluetoothGATTGetServicesRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTGetServicesRequest::encode(ProtoWriteBuffer buffer) const { buffer.encode_uint64(1, this->address); }
void BluetoothGATTGetServicesRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
}
bool BluetoothGATTDescriptor::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->uuid.push_back(value.as_uint64());
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTDescriptor::encode(ProtoWriteBuffer buffer) const {
  for (auto &it : this->uuid) {
    buffer.encode_uint64(1, it, true);
  }
  buffer.encode_uint32(2, this->handle);
}
void BluetoothGATTDescriptor::calculate_size(uint32_t &total_size) const {
  if (!this->uuid.empty()) {
    for (const auto &it : this->uuid) {
      ProtoSize::add_uint64_field(total_size, 1, it, true);
    }
  }
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
}
bool BluetoothGATTCharacteristic::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->uuid.push_back(value.as_uint64());
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    case 3: {
      this->properties = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool BluetoothGATTCharacteristic::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 4: {
      this->descriptors.push_back(value.as_message<BluetoothGATTDescriptor>());
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTCharacteristic::encode(ProtoWriteBuffer buffer) const {
  for (auto &it : this->uuid) {
    buffer.encode_uint64(1, it, true);
  }
  buffer.encode_uint32(2, this->handle);
  buffer.encode_uint32(3, this->properties);
  for (auto &it : this->descriptors) {
    buffer.encode_message<BluetoothGATTDescriptor>(4, it, true);
  }
}
void BluetoothGATTCharacteristic::calculate_size(uint32_t &total_size) const {
  if (!this->uuid.empty()) {
    for (const auto &it : this->uuid) {
      ProtoSize::add_uint64_field(total_size, 1, it, true);
    }
  }
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
  ProtoSize::add_uint32_field(total_size, 1, this->properties, false);
  ProtoSize::add_repeated_message(total_size, 1, this->descriptors);
}
bool BluetoothGATTService::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->uuid.push_back(value.as_uint64());
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool BluetoothGATTService::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3: {
      this->characteristics.push_back(value.as_message<BluetoothGATTCharacteristic>());
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTService::encode(ProtoWriteBuffer buffer) const {
  for (auto &it : this->uuid) {
    buffer.encode_uint64(1, it, true);
  }
  buffer.encode_uint32(2, this->handle);
  for (auto &it : this->characteristics) {
    buffer.encode_message<BluetoothGATTCharacteristic>(3, it, true);
  }
}
void BluetoothGATTService::calculate_size(uint32_t &total_size) const {
  if (!this->uuid.empty()) {
    for (const auto &it : this->uuid) {
      ProtoSize::add_uint64_field(total_size, 1, it, true);
    }
  }
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
  ProtoSize::add_repeated_message(total_size, 1, this->characteristics);
}
bool BluetoothGATTGetServicesResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    default:
      return false;
  }
}
bool BluetoothGATTGetServicesResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->services.push_back(value.as_message<BluetoothGATTService>());
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTGetServicesResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  for (auto &it : this->services) {
    buffer.encode_message<BluetoothGATTService>(2, it, true);
  }
}
void BluetoothGATTGetServicesResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_repeated_message(total_size, 1, this->services);
}
bool BluetoothGATTGetServicesDoneResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTGetServicesDoneResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
}
void BluetoothGATTGetServicesDoneResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
}
bool BluetoothGATTReadRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTReadRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
}
void BluetoothGATTReadRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
}
bool BluetoothGATTReadResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool BluetoothGATTReadResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3: {
      this->data = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTReadResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
  buffer.encode_bytes(3, reinterpret_cast<const uint8_t *>(this->data.data()), this->data.size());
}
void BluetoothGATTReadResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
  ProtoSize::add_string_field(total_size, 1, this->data, false);
}
bool BluetoothGATTWriteRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    case 3: {
      this->response = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool BluetoothGATTWriteRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 4: {
      this->data = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTWriteRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
  buffer.encode_bool(3, this->response);
  buffer.encode_bytes(4, reinterpret_cast<const uint8_t *>(this->data.data()), this->data.size());
}
void BluetoothGATTWriteRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
  ProtoSize::add_bool_field(total_size, 1, this->response, false);
  ProtoSize::add_string_field(total_size, 1, this->data, false);
}
bool BluetoothGATTReadDescriptorRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTReadDescriptorRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
}
void BluetoothGATTReadDescriptorRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
}
bool BluetoothGATTWriteDescriptorRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool BluetoothGATTWriteDescriptorRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3: {
      this->data = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTWriteDescriptorRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
  buffer.encode_bytes(3, reinterpret_cast<const uint8_t *>(this->data.data()), this->data.size());
}
void BluetoothGATTWriteDescriptorRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
  ProtoSize::add_string_field(total_size, 1, this->data, false);
}
bool BluetoothGATTNotifyRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    case 3: {
      this->enable = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTNotifyRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
  buffer.encode_bool(3, this->enable);
}
void BluetoothGATTNotifyRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
  ProtoSize::add_bool_field(total_size, 1, this->enable, false);
}
bool BluetoothGATTNotifyDataResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool BluetoothGATTNotifyDataResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3: {
      this->data = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTNotifyDataResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
  buffer.encode_bytes(3, reinterpret_cast<const uint8_t *>(this->data.data()), this->data.size());
}
void BluetoothGATTNotifyDataResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
  ProtoSize::add_string_field(total_size, 1, this->data, false);
}
bool BluetoothConnectionsFreeResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->free = value.as_uint32();
      return true;
    }
    case 2: {
      this->limit = value.as_uint32();
      return true;
    }
    case 3: {
      this->allocated.push_back(value.as_uint64());
      return true;
    }
    default:
      return false;
  }
}
void BluetoothConnectionsFreeResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->free);
  buffer.encode_uint32(2, this->limit);
  for (auto &it : this->allocated) {
    buffer.encode_uint64(3, it, true);
  }
}
void BluetoothConnectionsFreeResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint32_field(total_size, 1, this->free, false);
  ProtoSize::add_uint32_field(total_size, 1, this->limit, false);
  if (!this->allocated.empty()) {
    for (const auto &it : this->allocated) {
      ProtoSize::add_uint64_field(total_size, 1, it, true);
    }
  }
}
bool BluetoothGATTErrorResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    case 3: {
      this->error = value.as_int32();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTErrorResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
  buffer.encode_int32(3, this->error);
}
void BluetoothGATTErrorResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
  ProtoSize::add_int32_field(total_size, 1, this->error, false);
}
bool BluetoothGATTWriteResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTWriteResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
}
void BluetoothGATTWriteResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
}
bool BluetoothGATTNotifyResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->handle = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothGATTNotifyResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_uint32(2, this->handle);
}
void BluetoothGATTNotifyResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_uint32_field(total_size, 1, this->handle, false);
}
bool BluetoothDevicePairingResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->paired = value.as_bool();
      return true;
    }
    case 3: {
      this->error = value.as_int32();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothDevicePairingResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_bool(2, this->paired);
  buffer.encode_int32(3, this->error);
}
void BluetoothDevicePairingResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_bool_field(total_size, 1, this->paired, false);
  ProtoSize::add_int32_field(total_size, 1, this->error, false);
}
bool BluetoothDeviceUnpairingResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->success = value.as_bool();
      return true;
    }
    case 3: {
      this->error = value.as_int32();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothDeviceUnpairingResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_bool(2, this->success);
  buffer.encode_int32(3, this->error);
}
void BluetoothDeviceUnpairingResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_bool_field(total_size, 1, this->success, false);
  ProtoSize::add_int32_field(total_size, 1, this->error, false);
}
bool BluetoothDeviceClearCacheResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->address = value.as_uint64();
      return true;
    }
    case 2: {
      this->success = value.as_bool();
      return true;
    }
    case 3: {
      this->error = value.as_int32();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothDeviceClearCacheResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint64(1, this->address);
  buffer.encode_bool(2, this->success);
  buffer.encode_int32(3, this->error);
}
void BluetoothDeviceClearCacheResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint64_field(total_size, 1, this->address, false);
  ProtoSize::add_bool_field(total_size, 1, this->success, false);
  ProtoSize::add_int32_field(total_size, 1, this->error, false);
}
bool BluetoothScannerStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->state = value.as_enum<enums::BluetoothScannerState>();
      return true;
    }
    case 2: {
      this->mode = value.as_enum<enums::BluetoothScannerMode>();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothScannerStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_enum<enums::BluetoothScannerState>(1, this->state);
  buffer.encode_enum<enums::BluetoothScannerMode>(2, this->mode);
}
void BluetoothScannerStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->state), false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->mode), false);
}
bool BluetoothScannerSetModeRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->mode = value.as_enum<enums::BluetoothScannerMode>();
      return true;
    }
    default:
      return false;
  }
}
void BluetoothScannerSetModeRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_enum<enums::BluetoothScannerMode>(1, this->mode);
}
void BluetoothScannerSetModeRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->mode), false);
}
#endif
#ifdef USE_VOICE_ASSISTANT
bool SubscribeVoiceAssistantRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->subscribe = value.as_bool();
      return true;
    }
    case 2: {
      this->flags = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
void SubscribeVoiceAssistantRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_bool(1, this->subscribe);
  buffer.encode_uint32(2, this->flags);
}
void SubscribeVoiceAssistantRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_bool_field(total_size, 1, this->subscribe, false);
  ProtoSize::add_uint32_field(total_size, 1, this->flags, false);
}
bool VoiceAssistantAudioSettings::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->noise_suppression_level = value.as_uint32();
      return true;
    }
    case 2: {
      this->auto_gain = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool VoiceAssistantAudioSettings::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 3: {
      this->volume_multiplier = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantAudioSettings::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->noise_suppression_level);
  buffer.encode_uint32(2, this->auto_gain);
  buffer.encode_float(3, this->volume_multiplier);
}
void VoiceAssistantAudioSettings::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint32_field(total_size, 1, this->noise_suppression_level, false);
  ProtoSize::add_uint32_field(total_size, 1, this->auto_gain, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->volume_multiplier != 0.0f, false);
}
bool VoiceAssistantRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->start = value.as_bool();
      return true;
    }
    case 3: {
      this->flags = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool VoiceAssistantRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->conversation_id = value.as_string();
      return true;
    }
    case 4: {
      this->audio_settings = value.as_message<VoiceAssistantAudioSettings>();
      return true;
    }
    case 5: {
      this->wake_word_phrase = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_bool(1, this->start);
  buffer.encode_string(2, this->conversation_id);
  buffer.encode_uint32(3, this->flags);
  buffer.encode_message<VoiceAssistantAudioSettings>(4, this->audio_settings);
  buffer.encode_string(5, this->wake_word_phrase);
}
void VoiceAssistantRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_bool_field(total_size, 1, this->start, false);
  ProtoSize::add_string_field(total_size, 1, this->conversation_id, false);
  ProtoSize::add_uint32_field(total_size, 1, this->flags, false);
  ProtoSize::add_message_object(total_size, 1, this->audio_settings, false);
  ProtoSize::add_string_field(total_size, 1, this->wake_word_phrase, false);
}
bool VoiceAssistantResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->port = value.as_uint32();
      return true;
    }
    case 2: {
      this->error = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->port);
  buffer.encode_bool(2, this->error);
}
void VoiceAssistantResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_uint32_field(total_size, 1, this->port, false);
  ProtoSize::add_bool_field(total_size, 1, this->error, false);
}
bool VoiceAssistantEventData::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->name = value.as_string();
      return true;
    }
    case 2: {
      this->value = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantEventData::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->name);
  buffer.encode_string(2, this->value);
}
void VoiceAssistantEventData::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->value, false);
}
bool VoiceAssistantEventResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->event_type = value.as_enum<enums::VoiceAssistantEvent>();
      return true;
    }
    default:
      return false;
  }
}
bool VoiceAssistantEventResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->data.push_back(value.as_message<VoiceAssistantEventData>());
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantEventResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_enum<enums::VoiceAssistantEvent>(1, this->event_type);
  for (auto &it : this->data) {
    buffer.encode_message<VoiceAssistantEventData>(2, it, true);
  }
}
void VoiceAssistantEventResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->event_type), false);
  ProtoSize::add_repeated_message(total_size, 1, this->data);
}
bool VoiceAssistantAudio::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->end = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool VoiceAssistantAudio::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->data = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantAudio::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_bytes(1, reinterpret_cast<const uint8_t *>(this->data.data()), this->data.size());
  buffer.encode_bool(2, this->end);
}
void VoiceAssistantAudio::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->data, false);
  ProtoSize::add_bool_field(total_size, 1, this->end, false);
}
bool VoiceAssistantTimerEventResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->event_type = value.as_enum<enums::VoiceAssistantTimerEvent>();
      return true;
    }
    case 4: {
      this->total_seconds = value.as_uint32();
      return true;
    }
    case 5: {
      this->seconds_left = value.as_uint32();
      return true;
    }
    case 6: {
      this->is_active = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool VoiceAssistantTimerEventResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->timer_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantTimerEventResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_enum<enums::VoiceAssistantTimerEvent>(1, this->event_type);
  buffer.encode_string(2, this->timer_id);
  buffer.encode_string(3, this->name);
  buffer.encode_uint32(4, this->total_seconds);
  buffer.encode_uint32(5, this->seconds_left);
  buffer.encode_bool(6, this->is_active);
}
void VoiceAssistantTimerEventResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->event_type), false);
  ProtoSize::add_string_field(total_size, 1, this->timer_id, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_uint32_field(total_size, 1, this->total_seconds, false);
  ProtoSize::add_uint32_field(total_size, 1, this->seconds_left, false);
  ProtoSize::add_bool_field(total_size, 1, this->is_active, false);
}
bool VoiceAssistantAnnounceRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 4: {
      this->start_conversation = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool VoiceAssistantAnnounceRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->media_id = value.as_string();
      return true;
    }
    case 2: {
      this->text = value.as_string();
      return true;
    }
    case 3: {
      this->preannounce_media_id = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantAnnounceRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->media_id);
  buffer.encode_string(2, this->text);
  buffer.encode_string(3, this->preannounce_media_id);
  buffer.encode_bool(4, this->start_conversation);
}
void VoiceAssistantAnnounceRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->media_id, false);
  ProtoSize::add_string_field(total_size, 1, this->text, false);
  ProtoSize::add_string_field(total_size, 1, this->preannounce_media_id, false);
  ProtoSize::add_bool_field(total_size, 1, this->start_conversation, false);
}
bool VoiceAssistantAnnounceFinished::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->success = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantAnnounceFinished::encode(ProtoWriteBuffer buffer) const { buffer.encode_bool(1, this->success); }
void VoiceAssistantAnnounceFinished::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_bool_field(total_size, 1, this->success, false);
}
bool VoiceAssistantWakeWord::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->id = value.as_string();
      return true;
    }
    case 2: {
      this->wake_word = value.as_string();
      return true;
    }
    case 3: {
      this->trained_languages.push_back(value.as_string());
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantWakeWord::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->id);
  buffer.encode_string(2, this->wake_word);
  for (auto &it : this->trained_languages) {
    buffer.encode_string(3, it, true);
  }
}
void VoiceAssistantWakeWord::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->id, false);
  ProtoSize::add_string_field(total_size, 1, this->wake_word, false);
  if (!this->trained_languages.empty()) {
    for (const auto &it : this->trained_languages) {
      ProtoSize::add_string_field(total_size, 1, it, true);
    }
  }
}
bool VoiceAssistantConfigurationResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 3: {
      this->max_active_wake_words = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool VoiceAssistantConfigurationResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->available_wake_words.push_back(value.as_message<VoiceAssistantWakeWord>());
      return true;
    }
    case 2: {
      this->active_wake_words.push_back(value.as_string());
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantConfigurationResponse::encode(ProtoWriteBuffer buffer) const {
  for (auto &it : this->available_wake_words) {
    buffer.encode_message<VoiceAssistantWakeWord>(1, it, true);
  }
  for (auto &it : this->active_wake_words) {
    buffer.encode_string(2, it, true);
  }
  buffer.encode_uint32(3, this->max_active_wake_words);
}
void VoiceAssistantConfigurationResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_repeated_message(total_size, 1, this->available_wake_words);
  if (!this->active_wake_words.empty()) {
    for (const auto &it : this->active_wake_words) {
      ProtoSize::add_string_field(total_size, 1, it, true);
    }
  }
  ProtoSize::add_uint32_field(total_size, 1, this->max_active_wake_words, false);
}
bool VoiceAssistantSetConfiguration::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->active_wake_words.push_back(value.as_string());
      return true;
    }
    default:
      return false;
  }
}
void VoiceAssistantSetConfiguration::encode(ProtoWriteBuffer buffer) const {
  for (auto &it : this->active_wake_words) {
    buffer.encode_string(1, it, true);
  }
}
void VoiceAssistantSetConfiguration::calculate_size(uint32_t &total_size) const {
  if (!this->active_wake_words.empty()) {
    for (const auto &it : this->active_wake_words) {
      ProtoSize::add_string_field(total_size, 1, it, true);
    }
  }
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
bool ListEntitiesAlarmControlPanelResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 8: {
      this->supported_features = value.as_uint32();
      return true;
    }
    case 9: {
      this->requires_code = value.as_bool();
      return true;
    }
    case 10: {
      this->requires_code_to_arm = value.as_bool();
      return true;
    }
    case 11: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesAlarmControlPanelResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesAlarmControlPanelResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesAlarmControlPanelResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_uint32(8, this->supported_features);
  buffer.encode_bool(9, this->requires_code);
  buffer.encode_bool(10, this->requires_code_to_arm);
  buffer.encode_uint32(11, this->device_id);
}
void ListEntitiesAlarmControlPanelResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_uint32_field(total_size, 1, this->supported_features, false);
  ProtoSize::add_bool_field(total_size, 1, this->requires_code, false);
  ProtoSize::add_bool_field(total_size, 1, this->requires_code_to_arm, false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool AlarmControlPanelStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_enum<enums::AlarmControlPanelState>();
      return true;
    }
    default:
      return false;
  }
}
bool AlarmControlPanelStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void AlarmControlPanelStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_enum<enums::AlarmControlPanelState>(2, this->state);
}
void AlarmControlPanelStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->state), false);
}
bool AlarmControlPanelCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->command = value.as_enum<enums::AlarmControlPanelStateCommand>();
      return true;
    }
    default:
      return false;
  }
}
bool AlarmControlPanelCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 3: {
      this->code = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool AlarmControlPanelCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void AlarmControlPanelCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_enum<enums::AlarmControlPanelStateCommand>(2, this->command);
  buffer.encode_string(3, this->code);
}
void AlarmControlPanelCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->command), false);
  ProtoSize::add_string_field(total_size, 1, this->code, false);
}
#endif
#ifdef USE_TEXT
bool ListEntitiesTextResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 8: {
      this->min_length = value.as_uint32();
      return true;
    }
    case 9: {
      this->max_length = value.as_uint32();
      return true;
    }
    case 11: {
      this->mode = value.as_enum<enums::TextMode>();
      return true;
    }
    case 12: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesTextResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 10: {
      this->pattern = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesTextResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesTextResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_uint32(8, this->min_length);
  buffer.encode_uint32(9, this->max_length);
  buffer.encode_string(10, this->pattern);
  buffer.encode_enum<enums::TextMode>(11, this->mode);
  buffer.encode_uint32(12, this->device_id);
}
void ListEntitiesTextResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_uint32_field(total_size, 1, this->min_length, false);
  ProtoSize::add_uint32_field(total_size, 1, this->max_length, false);
  ProtoSize::add_string_field(total_size, 1, this->pattern, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->mode), false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool TextStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 3: {
      this->missing_state = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool TextStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool TextStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void TextStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_string(2, this->state);
  buffer.encode_bool(3, this->missing_state);
}
void TextStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->state, false);
  ProtoSize::add_bool_field(total_size, 1, this->missing_state, false);
}
bool TextCommandRequest::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool TextCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void TextCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_string(2, this->state);
}
void TextCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->state, false);
}
#endif
#ifdef USE_DATETIME_DATE
bool ListEntitiesDateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 8: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesDateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesDateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesDateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_uint32(8, this->device_id);
}
void ListEntitiesDateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool DateStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->missing_state = value.as_bool();
      return true;
    }
    case 3: {
      this->year = value.as_uint32();
      return true;
    }
    case 4: {
      this->month = value.as_uint32();
      return true;
    }
    case 5: {
      this->day = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool DateStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void DateStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->missing_state);
  buffer.encode_uint32(3, this->year);
  buffer.encode_uint32(4, this->month);
  buffer.encode_uint32(5, this->day);
}
void DateStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->missing_state, false);
  ProtoSize::add_uint32_field(total_size, 1, this->year, false);
  ProtoSize::add_uint32_field(total_size, 1, this->month, false);
  ProtoSize::add_uint32_field(total_size, 1, this->day, false);
}
bool DateCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->year = value.as_uint32();
      return true;
    }
    case 3: {
      this->month = value.as_uint32();
      return true;
    }
    case 4: {
      this->day = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool DateCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void DateCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_uint32(2, this->year);
  buffer.encode_uint32(3, this->month);
  buffer.encode_uint32(4, this->day);
}
void DateCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_uint32_field(total_size, 1, this->year, false);
  ProtoSize::add_uint32_field(total_size, 1, this->month, false);
  ProtoSize::add_uint32_field(total_size, 1, this->day, false);
}
#endif
#ifdef USE_DATETIME_TIME
bool ListEntitiesTimeResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 8: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesTimeResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesTimeResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesTimeResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_uint32(8, this->device_id);
}
void ListEntitiesTimeResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool TimeStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->missing_state = value.as_bool();
      return true;
    }
    case 3: {
      this->hour = value.as_uint32();
      return true;
    }
    case 4: {
      this->minute = value.as_uint32();
      return true;
    }
    case 5: {
      this->second = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool TimeStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void TimeStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->missing_state);
  buffer.encode_uint32(3, this->hour);
  buffer.encode_uint32(4, this->minute);
  buffer.encode_uint32(5, this->second);
}
void TimeStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->missing_state, false);
  ProtoSize::add_uint32_field(total_size, 1, this->hour, false);
  ProtoSize::add_uint32_field(total_size, 1, this->minute, false);
  ProtoSize::add_uint32_field(total_size, 1, this->second, false);
}
bool TimeCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->hour = value.as_uint32();
      return true;
    }
    case 3: {
      this->minute = value.as_uint32();
      return true;
    }
    case 4: {
      this->second = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool TimeCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void TimeCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_uint32(2, this->hour);
  buffer.encode_uint32(3, this->minute);
  buffer.encode_uint32(4, this->second);
}
void TimeCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_uint32_field(total_size, 1, this->hour, false);
  ProtoSize::add_uint32_field(total_size, 1, this->minute, false);
  ProtoSize::add_uint32_field(total_size, 1, this->second, false);
}
#endif
#ifdef USE_EVENT
bool ListEntitiesEventResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 10: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesEventResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 8: {
      this->device_class = value.as_string();
      return true;
    }
    case 9: {
      this->event_types.push_back(value.as_string());
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesEventResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesEventResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_string(8, this->device_class);
  for (auto &it : this->event_types) {
    buffer.encode_string(9, it, true);
  }
  buffer.encode_uint32(10, this->device_id);
}
void ListEntitiesEventResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_string_field(total_size, 1, this->device_class, false);
  if (!this->event_types.empty()) {
    for (const auto &it : this->event_types) {
      ProtoSize::add_string_field(total_size, 1, it, true);
    }
  }
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool EventResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 2: {
      this->event_type = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool EventResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void EventResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_string(2, this->event_type);
}
void EventResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->event_type, false);
}
#endif
#ifdef USE_VALVE
bool ListEntitiesValveResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 9: {
      this->assumed_state = value.as_bool();
      return true;
    }
    case 10: {
      this->supports_position = value.as_bool();
      return true;
    }
    case 11: {
      this->supports_stop = value.as_bool();
      return true;
    }
    case 12: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesValveResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 8: {
      this->device_class = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesValveResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesValveResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_string(8, this->device_class);
  buffer.encode_bool(9, this->assumed_state);
  buffer.encode_bool(10, this->supports_position);
  buffer.encode_bool(11, this->supports_stop);
  buffer.encode_uint32(12, this->device_id);
}
void ListEntitiesValveResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_string_field(total_size, 1, this->device_class, false);
  ProtoSize::add_bool_field(total_size, 1, this->assumed_state, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_position, false);
  ProtoSize::add_bool_field(total_size, 1, this->supports_stop, false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool ValveStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 3: {
      this->current_operation = value.as_enum<enums::ValveOperation>();
      return true;
    }
    default:
      return false;
  }
}
bool ValveStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 2: {
      this->position = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void ValveStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_float(2, this->position);
  buffer.encode_enum<enums::ValveOperation>(3, this->current_operation);
}
void ValveStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->position != 0.0f, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->current_operation), false);
}
bool ValveCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->has_position = value.as_bool();
      return true;
    }
    case 4: {
      this->stop = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool ValveCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 3: {
      this->position = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void ValveCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->has_position);
  buffer.encode_float(3, this->position);
  buffer.encode_bool(4, this->stop);
}
void ValveCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_position, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->position != 0.0f, false);
  ProtoSize::add_bool_field(total_size, 1, this->stop, false);
}
#endif
#ifdef USE_DATETIME_DATETIME
bool ListEntitiesDateTimeResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 8: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesDateTimeResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesDateTimeResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesDateTimeResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_uint32(8, this->device_id);
}
void ListEntitiesDateTimeResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool DateTimeStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->missing_state = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool DateTimeStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 3: {
      this->epoch_seconds = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void DateTimeStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->missing_state);
  buffer.encode_fixed32(3, this->epoch_seconds);
}
void DateTimeStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->missing_state, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->epoch_seconds != 0, false);
}
bool DateTimeCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 2: {
      this->epoch_seconds = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void DateTimeCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_fixed32(2, this->epoch_seconds);
}
void DateTimeCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->epoch_seconds != 0, false);
}
#endif
#ifdef USE_UPDATE
bool ListEntitiesUpdateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->disabled_by_default = value.as_bool();
      return true;
    }
    case 7: {
      this->entity_category = value.as_enum<enums::EntityCategory>();
      return true;
    }
    case 9: {
      this->device_id = value.as_uint32();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesUpdateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->object_id = value.as_string();
      return true;
    }
    case 3: {
      this->name = value.as_string();
      return true;
    }
    case 4: {
      this->unique_id = value.as_string();
      return true;
    }
    case 5: {
      this->icon = value.as_string();
      return true;
    }
    case 8: {
      this->device_class = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool ListEntitiesUpdateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 2: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void ListEntitiesUpdateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->object_id);
  buffer.encode_fixed32(2, this->key);
  buffer.encode_string(3, this->name);
  buffer.encode_string(4, this->unique_id);
  buffer.encode_string(5, this->icon);
  buffer.encode_bool(6, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(7, this->entity_category);
  buffer.encode_string(8, this->device_class);
  buffer.encode_uint32(9, this->device_id);
}
void ListEntitiesUpdateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_string_field(total_size, 1, this->object_id, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_string_field(total_size, 1, this->name, false);
  ProtoSize::add_string_field(total_size, 1, this->unique_id, false);
  ProtoSize::add_string_field(total_size, 1, this->icon, false);
  ProtoSize::add_bool_field(total_size, 1, this->disabled_by_default, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->entity_category), false);
  ProtoSize::add_string_field(total_size, 1, this->device_class, false);
  ProtoSize::add_uint32_field(total_size, 1, this->device_id, false);
}
bool UpdateStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->missing_state = value.as_bool();
      return true;
    }
    case 3: {
      this->in_progress = value.as_bool();
      return true;
    }
    case 4: {
      this->has_progress = value.as_bool();
      return true;
    }
    default:
      return false;
  }
}
bool UpdateStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 6: {
      this->current_version = value.as_string();
      return true;
    }
    case 7: {
      this->latest_version = value.as_string();
      return true;
    }
    case 8: {
      this->title = value.as_string();
      return true;
    }
    case 9: {
      this->release_summary = value.as_string();
      return true;
    }
    case 10: {
      this->release_url = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
bool UpdateStateResponse::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    case 5: {
      this->progress = value.as_float();
      return true;
    }
    default:
      return false;
  }
}
void UpdateStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->missing_state);
  buffer.encode_bool(3, this->in_progress);
  buffer.encode_bool(4, this->has_progress);
  buffer.encode_float(5, this->progress);
  buffer.encode_string(6, this->current_version);
  buffer.encode_string(7, this->latest_version);
  buffer.encode_string(8, this->title);
  buffer.encode_string(9, this->release_summary);
  buffer.encode_string(10, this->release_url);
}
void UpdateStateResponse::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_bool_field(total_size, 1, this->missing_state, false);
  ProtoSize::add_bool_field(total_size, 1, this->in_progress, false);
  ProtoSize::add_bool_field(total_size, 1, this->has_progress, false);
  ProtoSize::add_fixed_field<4>(total_size, 1, this->progress != 0.0f, false);
  ProtoSize::add_string_field(total_size, 1, this->current_version, false);
  ProtoSize::add_string_field(total_size, 1, this->latest_version, false);
  ProtoSize::add_string_field(total_size, 1, this->title, false);
  ProtoSize::add_string_field(total_size, 1, this->release_summary, false);
  ProtoSize::add_string_field(total_size, 1, this->release_url, false);
}
bool UpdateCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->command = value.as_enum<enums::UpdateCommand>();
      return true;
    }
    default:
      return false;
  }
}
bool UpdateCommandRequest::decode_32bit(uint32_t field_id, Proto32Bit value) {
  switch (field_id) {
    case 1: {
      this->key = value.as_fixed32();
      return true;
    }
    default:
      return false;
  }
}
void UpdateCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_enum<enums::UpdateCommand>(2, this->command);
}
void UpdateCommandRequest::calculate_size(uint32_t &total_size) const {
  ProtoSize::add_fixed_field<4>(total_size, 1, this->key != 0, false);
  ProtoSize::add_enum_field(total_size, 1, static_cast<uint32_t>(this->command), false);
}
#endif

}  // namespace api
}  // namespace esphome
