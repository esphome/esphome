#include "api_pb2.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

template<> const char *proto_enum_to_string<EnumLegacyCoverState>(EnumLegacyCoverState value) {
  switch (value) {
    case LEGACY_COVER_STATE_OPEN:
      return "LEGACY_COVER_STATE_OPEN";
    case LEGACY_COVER_STATE_CLOSED:
      return "LEGACY_COVER_STATE_CLOSED";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<EnumCoverOperation>(EnumCoverOperation value) {
  switch (value) {
    case COVER_OPERATION_IDLE:
      return "COVER_OPERATION_IDLE";
    case COVER_OPERATION_IS_OPENING:
      return "COVER_OPERATION_IS_OPENING";
    case COVER_OPERATION_IS_CLOSING:
      return "COVER_OPERATION_IS_CLOSING";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<EnumLegacyCoverCommand>(EnumLegacyCoverCommand value) {
  switch (value) {
    case LEGACY_COVER_COMMAND_OPEN:
      return "LEGACY_COVER_COMMAND_OPEN";
    case LEGACY_COVER_COMMAND_CLOSE:
      return "LEGACY_COVER_COMMAND_CLOSE";
    case LEGACY_COVER_COMMAND_STOP:
      return "LEGACY_COVER_COMMAND_STOP";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<EnumFanSpeed>(EnumFanSpeed value) {
  switch (value) {
    case FAN_SPEED_LOW:
      return "FAN_SPEED_LOW";
    case FAN_SPEED_MEDIUM:
      return "FAN_SPEED_MEDIUM";
    case FAN_SPEED_HIGH:
      return "FAN_SPEED_HIGH";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<EnumLogLevel>(EnumLogLevel value) {
  switch (value) {
    case LOG_LEVEL_NONE:
      return "LOG_LEVEL_NONE";
    case LOG_LEVEL_ERROR:
      return "LOG_LEVEL_ERROR";
    case LOG_LEVEL_WARN:
      return "LOG_LEVEL_WARN";
    case LOG_LEVEL_INFO:
      return "LOG_LEVEL_INFO";
    case LOG_LEVEL_DEBUG:
      return "LOG_LEVEL_DEBUG";
    case LOG_LEVEL_VERBOSE:
      return "LOG_LEVEL_VERBOSE";
    case LOG_LEVEL_VERY_VERBOSE:
      return "LOG_LEVEL_VERY_VERBOSE";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<EnumServiceArgType>(EnumServiceArgType value) {
  switch (value) {
    case SERVICE_ARG_TYPE_BOOL:
      return "SERVICE_ARG_TYPE_BOOL";
    case SERVICE_ARG_TYPE_INT:
      return "SERVICE_ARG_TYPE_INT";
    case SERVICE_ARG_TYPE_FLOAT:
      return "SERVICE_ARG_TYPE_FLOAT";
    case SERVICE_ARG_TYPE_STRING:
      return "SERVICE_ARG_TYPE_STRING";
    case SERVICE_ARG_TYPE_BOOL_ARRAY:
      return "SERVICE_ARG_TYPE_BOOL_ARRAY";
    case SERVICE_ARG_TYPE_INT_ARRAY:
      return "SERVICE_ARG_TYPE_INT_ARRAY";
    case SERVICE_ARG_TYPE_FLOAT_ARRAY:
      return "SERVICE_ARG_TYPE_FLOAT_ARRAY";
    case SERVICE_ARG_TYPE_STRING_ARRAY:
      return "SERVICE_ARG_TYPE_STRING_ARRAY";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<EnumClimateMode>(EnumClimateMode value) {
  switch (value) {
    case CLIMATE_MODE_OFF:
      return "CLIMATE_MODE_OFF";
    case CLIMATE_MODE_AUTO:
      return "CLIMATE_MODE_AUTO";
    case CLIMATE_MODE_COOL:
      return "CLIMATE_MODE_COOL";
    case CLIMATE_MODE_HEAT:
      return "CLIMATE_MODE_HEAT";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<EnumClimateAction>(EnumClimateAction value) {
  switch (value) {
    case CLIMATE_ACTION_OFF:
      return "CLIMATE_ACTION_OFF";
    case CLIMATE_ACTION_COOLING:
      return "CLIMATE_ACTION_COOLING";
    case CLIMATE_ACTION_HEATING:
      return "CLIMATE_ACTION_HEATING";
    default:
      return "UNKNOWN";
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
void HelloRequest::encode(ProtoWriteBuffer buffer) const { buffer.encode_string(1, this->client_info); }
void HelloRequest::dump_to(std::string &out) const {
  char buffer[64];
  out.append("HelloRequest {\n");
  out.append("  client_info: ");
  out.append("'").append(this->client_info).append("'");
  out.append("\n");
  out.append("}");
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
    default:
      return false;
  }
}
void HelloResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_uint32(1, this->api_version_major);
  buffer.encode_uint32(2, this->api_version_minor);
  buffer.encode_string(3, this->server_info);
}
void HelloResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("HelloResponse {\n");
  out.append("  api_version_major: ");
  sprintf(buffer, "%u", this->api_version_major);
  out.append(buffer);
  out.append("\n");

  out.append("  api_version_minor: ");
  sprintf(buffer, "%u", this->api_version_minor);
  out.append(buffer);
  out.append("\n");

  out.append("  server_info: ");
  out.append("'").append(this->server_info).append("'");
  out.append("\n");
  out.append("}");
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
void ConnectRequest::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ConnectRequest {\n");
  out.append("  password: ");
  out.append("'").append(this->password).append("'");
  out.append("\n");
  out.append("}");
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
void ConnectResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ConnectResponse {\n");
  out.append("  invalid_password: ");
  out.append(YESNO(this->invalid_password));
  out.append("\n");
  out.append("}");
}
void DisconnectRequest::encode(ProtoWriteBuffer buffer) const {}
void DisconnectRequest::dump_to(std::string &out) const { out.append("DisconnectRequest {}"); }
void DisconnectResponse::encode(ProtoWriteBuffer buffer) const {}
void DisconnectResponse::dump_to(std::string &out) const { out.append("DisconnectResponse {}"); }
void PingRequest::encode(ProtoWriteBuffer buffer) const {}
void PingRequest::dump_to(std::string &out) const { out.append("PingRequest {}"); }
void PingResponse::encode(ProtoWriteBuffer buffer) const {}
void PingResponse::dump_to(std::string &out) const { out.append("PingResponse {}"); }
void DeviceInfoRequest::encode(ProtoWriteBuffer buffer) const {}
void DeviceInfoRequest::dump_to(std::string &out) const { out.append("DeviceInfoRequest {}"); }
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
}
void DeviceInfoResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("DeviceInfoResponse {\n");
  out.append("  uses_password: ");
  out.append(YESNO(this->uses_password));
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  mac_address: ");
  out.append("'").append(this->mac_address).append("'");
  out.append("\n");

  out.append("  esphome_version: ");
  out.append("'").append(this->esphome_version).append("'");
  out.append("\n");

  out.append("  compilation_time: ");
  out.append("'").append(this->compilation_time).append("'");
  out.append("\n");

  out.append("  model: ");
  out.append("'").append(this->model).append("'");
  out.append("\n");

  out.append("  has_deep_sleep: ");
  out.append(YESNO(this->has_deep_sleep));
  out.append("\n");
  out.append("}");
}
void ListEntitiesRequest::encode(ProtoWriteBuffer buffer) const {}
void ListEntitiesRequest::dump_to(std::string &out) const { out.append("ListEntitiesRequest {}"); }
void ListEntitiesDoneResponse::encode(ProtoWriteBuffer buffer) const {}
void ListEntitiesDoneResponse::dump_to(std::string &out) const { out.append("ListEntitiesDoneResponse {}"); }
void SubscribeStatesRequest::encode(ProtoWriteBuffer buffer) const {}
void SubscribeStatesRequest::dump_to(std::string &out) const { out.append("SubscribeStatesRequest {}"); }
bool ListEntitiesBinarySensorResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->is_status_binary_sensor = value.as_bool();
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
}
void ListEntitiesBinarySensorResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesBinarySensorResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  unique_id: ");
  out.append("'").append(this->unique_id).append("'");
  out.append("\n");

  out.append("  device_class: ");
  out.append("'").append(this->device_class).append("'");
  out.append("\n");

  out.append("  is_status_binary_sensor: ");
  out.append(YESNO(this->is_status_binary_sensor));
  out.append("\n");
  out.append("}");
}
bool BinarySensorStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_bool();
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
}
void BinarySensorStateResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("BinarySensorStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");
  out.append("}");
}
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
}
void ListEntitiesCoverResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesCoverResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  unique_id: ");
  out.append("'").append(this->unique_id).append("'");
  out.append("\n");

  out.append("  assumed_state: ");
  out.append(YESNO(this->assumed_state));
  out.append("\n");

  out.append("  supports_position: ");
  out.append(YESNO(this->supports_position));
  out.append("\n");

  out.append("  supports_tilt: ");
  out.append(YESNO(this->supports_tilt));
  out.append("\n");

  out.append("  device_class: ");
  out.append("'").append(this->device_class).append("'");
  out.append("\n");
  out.append("}");
}
bool CoverStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->legacy_state = value.as_enum<EnumLegacyCoverState>();
      return true;
    }
    case 5: {
      this->current_operation = value.as_enum<EnumCoverOperation>();
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
  buffer.encode_enum<EnumLegacyCoverState>(2, this->legacy_state);
  buffer.encode_float(3, this->position);
  buffer.encode_float(4, this->tilt);
  buffer.encode_enum<EnumCoverOperation>(5, this->current_operation);
}
void CoverStateResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("CoverStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  legacy_state: ");
  out.append(proto_enum_to_string<EnumLegacyCoverState>(this->legacy_state));
  out.append("\n");

  out.append("  position: ");
  sprintf(buffer, "%g", this->position);
  out.append(buffer);
  out.append("\n");

  out.append("  tilt: ");
  sprintf(buffer, "%g", this->tilt);
  out.append(buffer);
  out.append("\n");

  out.append("  current_operation: ");
  out.append(proto_enum_to_string<EnumCoverOperation>(this->current_operation));
  out.append("\n");
  out.append("}");
}
bool CoverCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->has_legacy_command = value.as_bool();
      return true;
    }
    case 3: {
      this->legacy_command = value.as_enum<EnumLegacyCoverCommand>();
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
  buffer.encode_enum<EnumLegacyCoverCommand>(3, this->legacy_command);
  buffer.encode_bool(4, this->has_position);
  buffer.encode_float(5, this->position);
  buffer.encode_bool(6, this->has_tilt);
  buffer.encode_float(7, this->tilt);
  buffer.encode_bool(8, this->stop);
}
void CoverCommandRequest::dump_to(std::string &out) const {
  char buffer[64];
  out.append("CoverCommandRequest {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  has_legacy_command: ");
  out.append(YESNO(this->has_legacy_command));
  out.append("\n");

  out.append("  legacy_command: ");
  out.append(proto_enum_to_string<EnumLegacyCoverCommand>(this->legacy_command));
  out.append("\n");

  out.append("  has_position: ");
  out.append(YESNO(this->has_position));
  out.append("\n");

  out.append("  position: ");
  sprintf(buffer, "%g", this->position);
  out.append(buffer);
  out.append("\n");

  out.append("  has_tilt: ");
  out.append(YESNO(this->has_tilt));
  out.append("\n");

  out.append("  tilt: ");
  sprintf(buffer, "%g", this->tilt);
  out.append(buffer);
  out.append("\n");

  out.append("  stop: ");
  out.append(YESNO(this->stop));
  out.append("\n");
  out.append("}");
}
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
}
void ListEntitiesFanResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesFanResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  unique_id: ");
  out.append("'").append(this->unique_id).append("'");
  out.append("\n");

  out.append("  supports_oscillation: ");
  out.append(YESNO(this->supports_oscillation));
  out.append("\n");

  out.append("  supports_speed: ");
  out.append(YESNO(this->supports_speed));
  out.append("\n");
  out.append("}");
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
      this->speed = value.as_enum<EnumFanSpeed>();
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
  buffer.encode_enum<EnumFanSpeed>(4, this->speed);
}
void FanStateResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("FanStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");

  out.append("  oscillating: ");
  out.append(YESNO(this->oscillating));
  out.append("\n");

  out.append("  speed: ");
  out.append(proto_enum_to_string<EnumFanSpeed>(this->speed));
  out.append("\n");
  out.append("}");
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
      this->speed = value.as_enum<EnumFanSpeed>();
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
  buffer.encode_enum<EnumFanSpeed>(5, this->speed);
  buffer.encode_bool(6, this->has_oscillating);
  buffer.encode_bool(7, this->oscillating);
}
void FanCommandRequest::dump_to(std::string &out) const {
  char buffer[64];
  out.append("FanCommandRequest {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  has_state: ");
  out.append(YESNO(this->has_state));
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");

  out.append("  has_speed: ");
  out.append(YESNO(this->has_speed));
  out.append("\n");

  out.append("  speed: ");
  out.append(proto_enum_to_string<EnumFanSpeed>(this->speed));
  out.append("\n");

  out.append("  has_oscillating: ");
  out.append(YESNO(this->has_oscillating));
  out.append("\n");

  out.append("  oscillating: ");
  out.append(YESNO(this->oscillating));
  out.append("\n");
  out.append("}");
}
bool ListEntitiesLightResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 5: {
      this->supports_brightness = value.as_bool();
      return true;
    }
    case 6: {
      this->supports_rgb = value.as_bool();
      return true;
    }
    case 7: {
      this->supports_white_value = value.as_bool();
      return true;
    }
    case 8: {
      this->supports_color_temperature = value.as_bool();
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
  buffer.encode_bool(5, this->supports_brightness);
  buffer.encode_bool(6, this->supports_rgb);
  buffer.encode_bool(7, this->supports_white_value);
  buffer.encode_bool(8, this->supports_color_temperature);
  buffer.encode_float(9, this->min_mireds);
  buffer.encode_float(10, this->max_mireds);
  for (auto &it : this->effects) {
    buffer.encode_string(11, it, true);
  }
}
void ListEntitiesLightResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesLightResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  unique_id: ");
  out.append("'").append(this->unique_id).append("'");
  out.append("\n");

  out.append("  supports_brightness: ");
  out.append(YESNO(this->supports_brightness));
  out.append("\n");

  out.append("  supports_rgb: ");
  out.append(YESNO(this->supports_rgb));
  out.append("\n");

  out.append("  supports_white_value: ");
  out.append(YESNO(this->supports_white_value));
  out.append("\n");

  out.append("  supports_color_temperature: ");
  out.append(YESNO(this->supports_color_temperature));
  out.append("\n");

  out.append("  min_mireds: ");
  sprintf(buffer, "%g", this->min_mireds);
  out.append(buffer);
  out.append("\n");

  out.append("  max_mireds: ");
  sprintf(buffer, "%g", this->max_mireds);
  out.append(buffer);
  out.append("\n");

  for (const auto &it : this->effects) {
    out.append("  effects: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }
  out.append("}");
}
bool LightStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->state = value.as_bool();
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
    default:
      return false;
  }
}
void LightStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->state);
  buffer.encode_float(3, this->brightness);
  buffer.encode_float(4, this->red);
  buffer.encode_float(5, this->green);
  buffer.encode_float(6, this->blue);
  buffer.encode_float(7, this->white);
  buffer.encode_float(8, this->color_temperature);
  buffer.encode_string(9, this->effect);
}
void LightStateResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("LightStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");

  out.append("  brightness: ");
  sprintf(buffer, "%g", this->brightness);
  out.append(buffer);
  out.append("\n");

  out.append("  red: ");
  sprintf(buffer, "%g", this->red);
  out.append(buffer);
  out.append("\n");

  out.append("  green: ");
  sprintf(buffer, "%g", this->green);
  out.append(buffer);
  out.append("\n");

  out.append("  blue: ");
  sprintf(buffer, "%g", this->blue);
  out.append(buffer);
  out.append("\n");

  out.append("  white: ");
  sprintf(buffer, "%g", this->white);
  out.append(buffer);
  out.append("\n");

  out.append("  color_temperature: ");
  sprintf(buffer, "%g", this->color_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  effect: ");
  out.append("'").append(this->effect).append("'");
  out.append("\n");
  out.append("}");
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
  buffer.encode_bool(6, this->has_rgb);
  buffer.encode_float(7, this->red);
  buffer.encode_float(8, this->green);
  buffer.encode_float(9, this->blue);
  buffer.encode_bool(10, this->has_white);
  buffer.encode_float(11, this->white);
  buffer.encode_bool(12, this->has_color_temperature);
  buffer.encode_float(13, this->color_temperature);
  buffer.encode_bool(14, this->has_transition_length);
  buffer.encode_uint32(15, this->transition_length);
  buffer.encode_bool(16, this->has_flash_length);
  buffer.encode_uint32(17, this->flash_length);
  buffer.encode_bool(18, this->has_effect);
  buffer.encode_string(19, this->effect);
}
void LightCommandRequest::dump_to(std::string &out) const {
  char buffer[64];
  out.append("LightCommandRequest {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  has_state: ");
  out.append(YESNO(this->has_state));
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");

  out.append("  has_brightness: ");
  out.append(YESNO(this->has_brightness));
  out.append("\n");

  out.append("  brightness: ");
  sprintf(buffer, "%g", this->brightness);
  out.append(buffer);
  out.append("\n");

  out.append("  has_rgb: ");
  out.append(YESNO(this->has_rgb));
  out.append("\n");

  out.append("  red: ");
  sprintf(buffer, "%g", this->red);
  out.append(buffer);
  out.append("\n");

  out.append("  green: ");
  sprintf(buffer, "%g", this->green);
  out.append(buffer);
  out.append("\n");

  out.append("  blue: ");
  sprintf(buffer, "%g", this->blue);
  out.append(buffer);
  out.append("\n");

  out.append("  has_white: ");
  out.append(YESNO(this->has_white));
  out.append("\n");

  out.append("  white: ");
  sprintf(buffer, "%g", this->white);
  out.append(buffer);
  out.append("\n");

  out.append("  has_color_temperature: ");
  out.append(YESNO(this->has_color_temperature));
  out.append("\n");

  out.append("  color_temperature: ");
  sprintf(buffer, "%g", this->color_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  has_transition_length: ");
  out.append(YESNO(this->has_transition_length));
  out.append("\n");

  out.append("  transition_length: ");
  sprintf(buffer, "%u", this->transition_length);
  out.append(buffer);
  out.append("\n");

  out.append("  has_flash_length: ");
  out.append(YESNO(this->has_flash_length));
  out.append("\n");

  out.append("  flash_length: ");
  sprintf(buffer, "%u", this->flash_length);
  out.append(buffer);
  out.append("\n");

  out.append("  has_effect: ");
  out.append(YESNO(this->has_effect));
  out.append("\n");

  out.append("  effect: ");
  out.append("'").append(this->effect).append("'");
  out.append("\n");
  out.append("}");
}
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
}
void ListEntitiesSensorResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesSensorResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  unique_id: ");
  out.append("'").append(this->unique_id).append("'");
  out.append("\n");

  out.append("  icon: ");
  out.append("'").append(this->icon).append("'");
  out.append("\n");

  out.append("  unit_of_measurement: ");
  out.append("'").append(this->unit_of_measurement).append("'");
  out.append("\n");

  out.append("  accuracy_decimals: ");
  sprintf(buffer, "%d", this->accuracy_decimals);
  out.append(buffer);
  out.append("\n");

  out.append("  force_update: ");
  out.append(YESNO(this->force_update));
  out.append("\n");
  out.append("}");
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
}
void SensorStateResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("SensorStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  sprintf(buffer, "%g", this->state);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
bool ListEntitiesSwitchResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 6: {
      this->assumed_state = value.as_bool();
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
}
void ListEntitiesSwitchResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesSwitchResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  unique_id: ");
  out.append("'").append(this->unique_id).append("'");
  out.append("\n");

  out.append("  icon: ");
  out.append("'").append(this->icon).append("'");
  out.append("\n");

  out.append("  assumed_state: ");
  out.append(YESNO(this->assumed_state));
  out.append("\n");
  out.append("}");
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
void SwitchStateResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("SwitchStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");
  out.append("}");
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
void SwitchCommandRequest::dump_to(std::string &out) const {
  char buffer[64];
  out.append("SwitchCommandRequest {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");
  out.append("}");
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
}
void ListEntitiesTextSensorResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesTextSensorResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  unique_id: ");
  out.append("'").append(this->unique_id).append("'");
  out.append("\n");

  out.append("  icon: ");
  out.append("'").append(this->icon).append("'");
  out.append("\n");
  out.append("}");
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
}
void TextSensorStateResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("TextSensorStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append("'").append(this->state).append("'");
  out.append("\n");
  out.append("}");
}
bool SubscribeLogsRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->level = value.as_enum<EnumLogLevel>();
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
  buffer.encode_enum<EnumLogLevel>(1, this->level);
  buffer.encode_bool(2, this->dump_config);
}
void SubscribeLogsRequest::dump_to(std::string &out) const {
  char buffer[64];
  out.append("SubscribeLogsRequest {\n");
  out.append("  level: ");
  out.append(proto_enum_to_string<EnumLogLevel>(this->level));
  out.append("\n");

  out.append("  dump_config: ");
  out.append(YESNO(this->dump_config));
  out.append("\n");
  out.append("}");
}
bool SubscribeLogsResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 1: {
      this->level = value.as_enum<EnumLogLevel>();
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
    case 2: {
      this->tag = value.as_string();
      return true;
    }
    case 3: {
      this->message = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void SubscribeLogsResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_enum<EnumLogLevel>(1, this->level);
  buffer.encode_string(2, this->tag);
  buffer.encode_string(3, this->message);
  buffer.encode_bool(4, this->send_failed);
}
void SubscribeLogsResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("SubscribeLogsResponse {\n");
  out.append("  level: ");
  out.append(proto_enum_to_string<EnumLogLevel>(this->level));
  out.append("\n");

  out.append("  tag: ");
  out.append("'").append(this->tag).append("'");
  out.append("\n");

  out.append("  message: ");
  out.append("'").append(this->message).append("'");
  out.append("\n");

  out.append("  send_failed: ");
  out.append(YESNO(this->send_failed));
  out.append("\n");
  out.append("}");
}
void SubscribeHomeassistantServicesRequest::encode(ProtoWriteBuffer buffer) const {}
void SubscribeHomeassistantServicesRequest::dump_to(std::string &out) const {
  out.append("SubscribeHomeassistantServicesRequest {}");
}
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
void HomeassistantServiceMap::dump_to(std::string &out) const {
  char buffer[64];
  out.append("HomeassistantServiceMap {\n");
  out.append("  key: ");
  out.append("'").append(this->key).append("'");
  out.append("\n");

  out.append("  value: ");
  out.append("'").append(this->value).append("'");
  out.append("\n");
  out.append("}");
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
void HomeassistantServiceResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("HomeassistantServiceResponse {\n");
  out.append("  service: ");
  out.append("'").append(this->service).append("'");
  out.append("\n");

  for (const auto &it : this->data) {
    out.append("  data: ");
    it.dump_to(out);
    out.append("\n");
  }

  for (const auto &it : this->data_template) {
    out.append("  data_template: ");
    it.dump_to(out);
    out.append("\n");
  }

  for (const auto &it : this->variables) {
    out.append("  variables: ");
    it.dump_to(out);
    out.append("\n");
  }

  out.append("  is_event: ");
  out.append(YESNO(this->is_event));
  out.append("\n");
  out.append("}");
}
void SubscribeHomeAssistantStatesRequest::encode(ProtoWriteBuffer buffer) const {}
void SubscribeHomeAssistantStatesRequest::dump_to(std::string &out) const {
  out.append("SubscribeHomeAssistantStatesRequest {}");
}
bool SubscribeHomeAssistantStateResponse::decode_length(uint32_t field_id, ProtoLengthDelimited value) {
  switch (field_id) {
    case 1: {
      this->entity_id = value.as_string();
      return true;
    }
    default:
      return false;
  }
}
void SubscribeHomeAssistantStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->entity_id);
}
void SubscribeHomeAssistantStateResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("SubscribeHomeAssistantStateResponse {\n");
  out.append("  entity_id: ");
  out.append("'").append(this->entity_id).append("'");
  out.append("\n");
  out.append("}");
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
    default:
      return false;
  }
}
void HomeAssistantStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_string(1, this->entity_id);
  buffer.encode_string(2, this->state);
}
void HomeAssistantStateResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("HomeAssistantStateResponse {\n");
  out.append("  entity_id: ");
  out.append("'").append(this->entity_id).append("'");
  out.append("\n");

  out.append("  state: ");
  out.append("'").append(this->state).append("'");
  out.append("\n");
  out.append("}");
}
void GetTimeRequest::encode(ProtoWriteBuffer buffer) const {}
void GetTimeRequest::dump_to(std::string &out) const { out.append("GetTimeRequest {}"); }
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
void GetTimeResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("GetTimeResponse {\n");
  out.append("  epoch_seconds: ");
  sprintf(buffer, "%u", this->epoch_seconds);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
bool ListEntitiesServicesArgument::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->type = value.as_enum<EnumServiceArgType>();
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
  buffer.encode_enum<EnumServiceArgType>(2, this->type);
}
void ListEntitiesServicesArgument::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesServicesArgument {\n");
  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  type: ");
  out.append(proto_enum_to_string<EnumServiceArgType>(this->type));
  out.append("\n");
  out.append("}");
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
void ListEntitiesServicesResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesServicesResponse {\n");
  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  for (const auto &it : this->args) {
    out.append("  args: ");
    it.dump_to(out);
    out.append("\n");
  }
  out.append("}");
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
void ExecuteServiceArgument::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ExecuteServiceArgument {\n");
  out.append("  bool_: ");
  out.append(YESNO(this->bool_));
  out.append("\n");

  out.append("  legacy_int: ");
  sprintf(buffer, "%d", this->legacy_int);
  out.append(buffer);
  out.append("\n");

  out.append("  float_: ");
  sprintf(buffer, "%g", this->float_);
  out.append(buffer);
  out.append("\n");

  out.append("  string_: ");
  out.append("'").append(this->string_).append("'");
  out.append("\n");

  out.append("  int_: ");
  sprintf(buffer, "%d", this->int_);
  out.append(buffer);
  out.append("\n");

  for (const auto it : this->bool_array) {
    out.append("  bool_array: ");
    out.append(YESNO(it));
    out.append("\n");
  }

  for (const auto &it : this->int_array) {
    out.append("  int_array: ");
    sprintf(buffer, "%d", it);
    out.append(buffer);
    out.append("\n");
  }

  for (const auto &it : this->float_array) {
    out.append("  float_array: ");
    sprintf(buffer, "%g", it);
    out.append(buffer);
    out.append("\n");
  }

  for (const auto &it : this->string_array) {
    out.append("  string_array: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }
  out.append("}");
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
void ExecuteServiceRequest::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ExecuteServiceRequest {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  for (const auto &it : this->args) {
    out.append("  args: ");
    it.dump_to(out);
    out.append("\n");
  }
  out.append("}");
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
}
void ListEntitiesCameraResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesCameraResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  unique_id: ");
  out.append("'").append(this->unique_id).append("'");
  out.append("\n");
  out.append("}");
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
  buffer.encode_string(2, this->data);
  buffer.encode_bool(3, this->done);
}
void CameraImageResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("CameraImageResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  data: ");
  out.append("'").append(this->data).append("'");
  out.append("\n");

  out.append("  done: ");
  out.append(YESNO(this->done));
  out.append("\n");
  out.append("}");
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
void CameraImageRequest::dump_to(std::string &out) const {
  char buffer[64];
  out.append("CameraImageRequest {\n");
  out.append("  single: ");
  out.append(YESNO(this->single));
  out.append("\n");

  out.append("  stream: ");
  out.append(YESNO(this->stream));
  out.append("\n");
  out.append("}");
}
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
      this->supported_modes.push_back(value.as_enum<EnumClimateMode>());
      return true;
    }
    case 11: {
      this->supports_away = value.as_bool();
      return true;
    }
    case 12: {
      this->supports_action = value.as_bool();
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
      this->visual_temperature_step = value.as_float();
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
    buffer.encode_enum<EnumClimateMode>(7, it, true);
  }
  buffer.encode_float(8, this->visual_min_temperature);
  buffer.encode_float(9, this->visual_max_temperature);
  buffer.encode_float(10, this->visual_temperature_step);
  buffer.encode_bool(11, this->supports_away);
  buffer.encode_bool(12, this->supports_action);
}
void ListEntitiesClimateResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesClimateResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  unique_id: ");
  out.append("'").append(this->unique_id).append("'");
  out.append("\n");

  out.append("  supports_current_temperature: ");
  out.append(YESNO(this->supports_current_temperature));
  out.append("\n");

  out.append("  supports_two_point_target_temperature: ");
  out.append(YESNO(this->supports_two_point_target_temperature));
  out.append("\n");

  for (const auto &it : this->supported_modes) {
    out.append("  supported_modes: ");
    out.append(proto_enum_to_string<EnumClimateMode>(it));
    out.append("\n");
  }

  out.append("  visual_min_temperature: ");
  sprintf(buffer, "%g", this->visual_min_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  visual_max_temperature: ");
  sprintf(buffer, "%g", this->visual_max_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  visual_temperature_step: ");
  sprintf(buffer, "%g", this->visual_temperature_step);
  out.append(buffer);
  out.append("\n");

  out.append("  supports_away: ");
  out.append(YESNO(this->supports_away));
  out.append("\n");

  out.append("  supports_action: ");
  out.append(YESNO(this->supports_action));
  out.append("\n");
  out.append("}");
}
bool ClimateStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->mode = value.as_enum<EnumClimateMode>();
      return true;
    }
    case 7: {
      this->away = value.as_bool();
      return true;
    }
    case 8: {
      this->action = value.as_enum<EnumClimateAction>();
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
    default:
      return false;
  }
}
void ClimateStateResponse::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_enum<EnumClimateMode>(2, this->mode);
  buffer.encode_float(3, this->current_temperature);
  buffer.encode_float(4, this->target_temperature);
  buffer.encode_float(5, this->target_temperature_low);
  buffer.encode_float(6, this->target_temperature_high);
  buffer.encode_bool(7, this->away);
  buffer.encode_enum<EnumClimateAction>(8, this->action);
}
void ClimateStateResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ClimateStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  mode: ");
  out.append(proto_enum_to_string<EnumClimateMode>(this->mode));
  out.append("\n");

  out.append("  current_temperature: ");
  sprintf(buffer, "%g", this->current_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  target_temperature: ");
  sprintf(buffer, "%g", this->target_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  target_temperature_low: ");
  sprintf(buffer, "%g", this->target_temperature_low);
  out.append(buffer);
  out.append("\n");

  out.append("  target_temperature_high: ");
  sprintf(buffer, "%g", this->target_temperature_high);
  out.append(buffer);
  out.append("\n");

  out.append("  away: ");
  out.append(YESNO(this->away));
  out.append("\n");

  out.append("  action: ");
  out.append(proto_enum_to_string<EnumClimateAction>(this->action));
  out.append("\n");
  out.append("}");
}
bool ClimateCommandRequest::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->has_mode = value.as_bool();
      return true;
    }
    case 3: {
      this->mode = value.as_enum<EnumClimateMode>();
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
      this->has_away = value.as_bool();
      return true;
    }
    case 11: {
      this->away = value.as_bool();
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
    default:
      return false;
  }
}
void ClimateCommandRequest::encode(ProtoWriteBuffer buffer) const {
  buffer.encode_fixed32(1, this->key);
  buffer.encode_bool(2, this->has_mode);
  buffer.encode_enum<EnumClimateMode>(3, this->mode);
  buffer.encode_bool(4, this->has_target_temperature);
  buffer.encode_float(5, this->target_temperature);
  buffer.encode_bool(6, this->has_target_temperature_low);
  buffer.encode_float(7, this->target_temperature_low);
  buffer.encode_bool(8, this->has_target_temperature_high);
  buffer.encode_float(9, this->target_temperature_high);
  buffer.encode_bool(10, this->has_away);
  buffer.encode_bool(11, this->away);
}
void ClimateCommandRequest::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ClimateCommandRequest {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  has_mode: ");
  out.append(YESNO(this->has_mode));
  out.append("\n");

  out.append("  mode: ");
  out.append(proto_enum_to_string<EnumClimateMode>(this->mode));
  out.append("\n");

  out.append("  has_target_temperature: ");
  out.append(YESNO(this->has_target_temperature));
  out.append("\n");

  out.append("  target_temperature: ");
  sprintf(buffer, "%g", this->target_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  has_target_temperature_low: ");
  out.append(YESNO(this->has_target_temperature_low));
  out.append("\n");

  out.append("  target_temperature_low: ");
  sprintf(buffer, "%g", this->target_temperature_low);
  out.append(buffer);
  out.append("\n");

  out.append("  has_target_temperature_high: ");
  out.append(YESNO(this->has_target_temperature_high));
  out.append("\n");

  out.append("  target_temperature_high: ");
  sprintf(buffer, "%g", this->target_temperature_high);
  out.append(buffer);
  out.append("\n");

  out.append("  has_away: ");
  out.append(YESNO(this->has_away));
  out.append("\n");

  out.append("  away: ");
  out.append(YESNO(this->away));
  out.append("\n");
  out.append("}");
}

}  // namespace api
}  // namespace esphome
