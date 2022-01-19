// This file was automatically generated with a tool.
// See scripts/api_protobuf/api_protobuf.py
#include "api_pb2.h"
#include "esphome/core/log.h"

namespace esphome {
namespace api {

template<> const char *proto_enum_to_string<enums::EntityCategory>(enums::EntityCategory value) {
  switch (value) {
    case enums::ENTITY_CATEGORY_NONE:
      return "ENTITY_CATEGORY_NONE";
    case enums::ENTITY_CATEGORY_CONFIG:
      return "ENTITY_CATEGORY_CONFIG";
    case enums::ENTITY_CATEGORY_DIAGNOSTIC:
      return "ENTITY_CATEGORY_DIAGNOSTIC";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::LegacyCoverState>(enums::LegacyCoverState value) {
  switch (value) {
    case enums::LEGACY_COVER_STATE_OPEN:
      return "LEGACY_COVER_STATE_OPEN";
    case enums::LEGACY_COVER_STATE_CLOSED:
      return "LEGACY_COVER_STATE_CLOSED";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::CoverOperation>(enums::CoverOperation value) {
  switch (value) {
    case enums::COVER_OPERATION_IDLE:
      return "COVER_OPERATION_IDLE";
    case enums::COVER_OPERATION_IS_OPENING:
      return "COVER_OPERATION_IS_OPENING";
    case enums::COVER_OPERATION_IS_CLOSING:
      return "COVER_OPERATION_IS_CLOSING";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::LegacyCoverCommand>(enums::LegacyCoverCommand value) {
  switch (value) {
    case enums::LEGACY_COVER_COMMAND_OPEN:
      return "LEGACY_COVER_COMMAND_OPEN";
    case enums::LEGACY_COVER_COMMAND_CLOSE:
      return "LEGACY_COVER_COMMAND_CLOSE";
    case enums::LEGACY_COVER_COMMAND_STOP:
      return "LEGACY_COVER_COMMAND_STOP";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::FanSpeed>(enums::FanSpeed value) {
  switch (value) {
    case enums::FAN_SPEED_LOW:
      return "FAN_SPEED_LOW";
    case enums::FAN_SPEED_MEDIUM:
      return "FAN_SPEED_MEDIUM";
    case enums::FAN_SPEED_HIGH:
      return "FAN_SPEED_HIGH";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::FanDirection>(enums::FanDirection value) {
  switch (value) {
    case enums::FAN_DIRECTION_FORWARD:
      return "FAN_DIRECTION_FORWARD";
    case enums::FAN_DIRECTION_REVERSE:
      return "FAN_DIRECTION_REVERSE";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::ColorMode>(enums::ColorMode value) {
  switch (value) {
    case enums::COLOR_MODE_UNKNOWN:
      return "COLOR_MODE_UNKNOWN";
    case enums::COLOR_MODE_ON_OFF:
      return "COLOR_MODE_ON_OFF";
    case enums::COLOR_MODE_BRIGHTNESS:
      return "COLOR_MODE_BRIGHTNESS";
    case enums::COLOR_MODE_WHITE:
      return "COLOR_MODE_WHITE";
    case enums::COLOR_MODE_COLOR_TEMPERATURE:
      return "COLOR_MODE_COLOR_TEMPERATURE";
    case enums::COLOR_MODE_COLD_WARM_WHITE:
      return "COLOR_MODE_COLD_WARM_WHITE";
    case enums::COLOR_MODE_RGB:
      return "COLOR_MODE_RGB";
    case enums::COLOR_MODE_RGB_WHITE:
      return "COLOR_MODE_RGB_WHITE";
    case enums::COLOR_MODE_RGB_COLOR_TEMPERATURE:
      return "COLOR_MODE_RGB_COLOR_TEMPERATURE";
    case enums::COLOR_MODE_RGB_COLD_WARM_WHITE:
      return "COLOR_MODE_RGB_COLD_WARM_WHITE";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::SensorStateClass>(enums::SensorStateClass value) {
  switch (value) {
    case enums::STATE_CLASS_NONE:
      return "STATE_CLASS_NONE";
    case enums::STATE_CLASS_MEASUREMENT:
      return "STATE_CLASS_MEASUREMENT";
    case enums::STATE_CLASS_TOTAL_INCREASING:
      return "STATE_CLASS_TOTAL_INCREASING";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::SensorLastResetType>(enums::SensorLastResetType value) {
  switch (value) {
    case enums::LAST_RESET_NONE:
      return "LAST_RESET_NONE";
    case enums::LAST_RESET_NEVER:
      return "LAST_RESET_NEVER";
    case enums::LAST_RESET_AUTO:
      return "LAST_RESET_AUTO";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::LogLevel>(enums::LogLevel value) {
  switch (value) {
    case enums::LOG_LEVEL_NONE:
      return "LOG_LEVEL_NONE";
    case enums::LOG_LEVEL_ERROR:
      return "LOG_LEVEL_ERROR";
    case enums::LOG_LEVEL_WARN:
      return "LOG_LEVEL_WARN";
    case enums::LOG_LEVEL_INFO:
      return "LOG_LEVEL_INFO";
    case enums::LOG_LEVEL_CONFIG:
      return "LOG_LEVEL_CONFIG";
    case enums::LOG_LEVEL_DEBUG:
      return "LOG_LEVEL_DEBUG";
    case enums::LOG_LEVEL_VERBOSE:
      return "LOG_LEVEL_VERBOSE";
    case enums::LOG_LEVEL_VERY_VERBOSE:
      return "LOG_LEVEL_VERY_VERBOSE";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::ServiceArgType>(enums::ServiceArgType value) {
  switch (value) {
    case enums::SERVICE_ARG_TYPE_BOOL:
      return "SERVICE_ARG_TYPE_BOOL";
    case enums::SERVICE_ARG_TYPE_INT:
      return "SERVICE_ARG_TYPE_INT";
    case enums::SERVICE_ARG_TYPE_FLOAT:
      return "SERVICE_ARG_TYPE_FLOAT";
    case enums::SERVICE_ARG_TYPE_STRING:
      return "SERVICE_ARG_TYPE_STRING";
    case enums::SERVICE_ARG_TYPE_BOOL_ARRAY:
      return "SERVICE_ARG_TYPE_BOOL_ARRAY";
    case enums::SERVICE_ARG_TYPE_INT_ARRAY:
      return "SERVICE_ARG_TYPE_INT_ARRAY";
    case enums::SERVICE_ARG_TYPE_FLOAT_ARRAY:
      return "SERVICE_ARG_TYPE_FLOAT_ARRAY";
    case enums::SERVICE_ARG_TYPE_STRING_ARRAY:
      return "SERVICE_ARG_TYPE_STRING_ARRAY";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::ClimateMode>(enums::ClimateMode value) {
  switch (value) {
    case enums::CLIMATE_MODE_OFF:
      return "CLIMATE_MODE_OFF";
    case enums::CLIMATE_MODE_HEAT_COOL:
      return "CLIMATE_MODE_HEAT_COOL";
    case enums::CLIMATE_MODE_COOL:
      return "CLIMATE_MODE_COOL";
    case enums::CLIMATE_MODE_HEAT:
      return "CLIMATE_MODE_HEAT";
    case enums::CLIMATE_MODE_FAN_ONLY:
      return "CLIMATE_MODE_FAN_ONLY";
    case enums::CLIMATE_MODE_DRY:
      return "CLIMATE_MODE_DRY";
    case enums::CLIMATE_MODE_AUTO:
      return "CLIMATE_MODE_AUTO";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::ClimateFanMode>(enums::ClimateFanMode value) {
  switch (value) {
    case enums::CLIMATE_FAN_ON:
      return "CLIMATE_FAN_ON";
    case enums::CLIMATE_FAN_OFF:
      return "CLIMATE_FAN_OFF";
    case enums::CLIMATE_FAN_AUTO:
      return "CLIMATE_FAN_AUTO";
    case enums::CLIMATE_FAN_LOW:
      return "CLIMATE_FAN_LOW";
    case enums::CLIMATE_FAN_MEDIUM:
      return "CLIMATE_FAN_MEDIUM";
    case enums::CLIMATE_FAN_HIGH:
      return "CLIMATE_FAN_HIGH";
    case enums::CLIMATE_FAN_MIDDLE:
      return "CLIMATE_FAN_MIDDLE";
    case enums::CLIMATE_FAN_FOCUS:
      return "CLIMATE_FAN_FOCUS";
    case enums::CLIMATE_FAN_DIFFUSE:
      return "CLIMATE_FAN_DIFFUSE";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::ClimateSwingMode>(enums::ClimateSwingMode value) {
  switch (value) {
    case enums::CLIMATE_SWING_OFF:
      return "CLIMATE_SWING_OFF";
    case enums::CLIMATE_SWING_BOTH:
      return "CLIMATE_SWING_BOTH";
    case enums::CLIMATE_SWING_VERTICAL:
      return "CLIMATE_SWING_VERTICAL";
    case enums::CLIMATE_SWING_HORIZONTAL:
      return "CLIMATE_SWING_HORIZONTAL";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::ClimateAction>(enums::ClimateAction value) {
  switch (value) {
    case enums::CLIMATE_ACTION_OFF:
      return "CLIMATE_ACTION_OFF";
    case enums::CLIMATE_ACTION_COOLING:
      return "CLIMATE_ACTION_COOLING";
    case enums::CLIMATE_ACTION_HEATING:
      return "CLIMATE_ACTION_HEATING";
    case enums::CLIMATE_ACTION_IDLE:
      return "CLIMATE_ACTION_IDLE";
    case enums::CLIMATE_ACTION_DRYING:
      return "CLIMATE_ACTION_DRYING";
    case enums::CLIMATE_ACTION_FAN:
      return "CLIMATE_ACTION_FAN";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::ClimatePreset>(enums::ClimatePreset value) {
  switch (value) {
    case enums::CLIMATE_PRESET_NONE:
      return "CLIMATE_PRESET_NONE";
    case enums::CLIMATE_PRESET_HOME:
      return "CLIMATE_PRESET_HOME";
    case enums::CLIMATE_PRESET_AWAY:
      return "CLIMATE_PRESET_AWAY";
    case enums::CLIMATE_PRESET_BOOST:
      return "CLIMATE_PRESET_BOOST";
    case enums::CLIMATE_PRESET_COMFORT:
      return "CLIMATE_PRESET_COMFORT";
    case enums::CLIMATE_PRESET_ECO:
      return "CLIMATE_PRESET_ECO";
    case enums::CLIMATE_PRESET_SLEEP:
      return "CLIMATE_PRESET_SLEEP";
    case enums::CLIMATE_PRESET_ACTIVITY:
      return "CLIMATE_PRESET_ACTIVITY";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::NumberMode>(enums::NumberMode value) {
  switch (value) {
    case enums::NUMBER_MODE_AUTO:
      return "NUMBER_MODE_AUTO";
    case enums::NUMBER_MODE_BOX:
      return "NUMBER_MODE_BOX";
    case enums::NUMBER_MODE_SLIDER:
      return "NUMBER_MODE_SLIDER";
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void HelloRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("HelloRequest {\n");
  out.append("  client_info: ");
  out.append("'").append(this->client_info).append("'");
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void HelloResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void ConnectRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ConnectRequest {\n");
  out.append("  password: ");
  out.append("'").append(this->password).append("'");
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void ConnectResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ConnectResponse {\n");
  out.append("  invalid_password: ");
  out.append(YESNO(this->invalid_password));
  out.append("\n");
  out.append("}");
}
#endif
void DisconnectRequest::encode(ProtoWriteBuffer buffer) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
void DisconnectRequest::dump_to(std::string &out) const { out.append("DisconnectRequest {}"); }
#endif
void DisconnectResponse::encode(ProtoWriteBuffer buffer) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
void DisconnectResponse::dump_to(std::string &out) const { out.append("DisconnectResponse {}"); }
#endif
void PingRequest::encode(ProtoWriteBuffer buffer) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
void PingRequest::dump_to(std::string &out) const { out.append("PingRequest {}"); }
#endif
void PingResponse::encode(ProtoWriteBuffer buffer) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
void PingResponse::dump_to(std::string &out) const { out.append("PingResponse {}"); }
#endif
void DeviceInfoRequest::encode(ProtoWriteBuffer buffer) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
void DeviceInfoRequest::dump_to(std::string &out) const { out.append("DeviceInfoRequest {}"); }
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void DeviceInfoResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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

  out.append("  project_name: ");
  out.append("'").append(this->project_name).append("'");
  out.append("\n");

  out.append("  project_version: ");
  out.append("'").append(this->project_version).append("'");
  out.append("\n");

  out.append("  webserver_port: ");
  sprintf(buffer, "%u", this->webserver_port);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
void ListEntitiesRequest::encode(ProtoWriteBuffer buffer) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesRequest::dump_to(std::string &out) const { out.append("ListEntitiesRequest {}"); }
#endif
void ListEntitiesDoneResponse::encode(ProtoWriteBuffer buffer) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesDoneResponse::dump_to(std::string &out) const { out.append("ListEntitiesDoneResponse {}"); }
#endif
void SubscribeStatesRequest::encode(ProtoWriteBuffer buffer) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
void SubscribeStatesRequest::dump_to(std::string &out) const { out.append("SubscribeStatesRequest {}"); }
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesBinarySensorResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  icon: ");
  out.append("'").append(this->icon).append("'");
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void BinarySensorStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BinarySensorStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");
  out.append("}");
}
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesCoverResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  icon: ");
  out.append("'").append(this->icon).append("'");
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void CoverStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("CoverStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  legacy_state: ");
  out.append(proto_enum_to_string<enums::LegacyCoverState>(this->legacy_state));
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
  out.append(proto_enum_to_string<enums::CoverOperation>(this->current_operation));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void CoverCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("CoverCommandRequest {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  has_legacy_command: ");
  out.append(YESNO(this->has_legacy_command));
  out.append("\n");

  out.append("  legacy_command: ");
  out.append(proto_enum_to_string<enums::LegacyCoverCommand>(this->legacy_command));
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
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesFanResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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

  out.append("  supports_direction: ");
  out.append(YESNO(this->supports_direction));
  out.append("\n");

  out.append("  supported_speed_count: ");
  sprintf(buffer, "%d", this->supported_speed_count);
  out.append(buffer);
  out.append("\n");

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  icon: ");
  out.append("'").append(this->icon).append("'");
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");
  out.append("}");
}
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void FanStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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
  out.append(proto_enum_to_string<enums::FanSpeed>(this->speed));
  out.append("\n");

  out.append("  direction: ");
  out.append(proto_enum_to_string<enums::FanDirection>(this->direction));
  out.append("\n");

  out.append("  speed_level: ");
  sprintf(buffer, "%d", this->speed_level);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void FanCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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
  out.append(proto_enum_to_string<enums::FanSpeed>(this->speed));
  out.append("\n");

  out.append("  has_oscillating: ");
  out.append(YESNO(this->has_oscillating));
  out.append("\n");

  out.append("  oscillating: ");
  out.append(YESNO(this->oscillating));
  out.append("\n");

  out.append("  has_direction: ");
  out.append(YESNO(this->has_direction));
  out.append("\n");

  out.append("  direction: ");
  out.append(proto_enum_to_string<enums::FanDirection>(this->direction));
  out.append("\n");

  out.append("  has_speed_level: ");
  out.append(YESNO(this->has_speed_level));
  out.append("\n");

  out.append("  speed_level: ");
  sprintf(buffer, "%d", this->speed_level);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesLightResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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

  for (const auto &it : this->supported_color_modes) {
    out.append("  supported_color_modes: ");
    out.append(proto_enum_to_string<enums::ColorMode>(it));
    out.append("\n");
  }

  out.append("  legacy_supports_brightness: ");
  out.append(YESNO(this->legacy_supports_brightness));
  out.append("\n");

  out.append("  legacy_supports_rgb: ");
  out.append(YESNO(this->legacy_supports_rgb));
  out.append("\n");

  out.append("  legacy_supports_white_value: ");
  out.append(YESNO(this->legacy_supports_white_value));
  out.append("\n");

  out.append("  legacy_supports_color_temperature: ");
  out.append(YESNO(this->legacy_supports_color_temperature));
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

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  icon: ");
  out.append("'").append(this->icon).append("'");
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void LightStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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

  out.append("  color_mode: ");
  out.append(proto_enum_to_string<enums::ColorMode>(this->color_mode));
  out.append("\n");

  out.append("  color_brightness: ");
  sprintf(buffer, "%g", this->color_brightness);
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

  out.append("  cold_white: ");
  sprintf(buffer, "%g", this->cold_white);
  out.append(buffer);
  out.append("\n");

  out.append("  warm_white: ");
  sprintf(buffer, "%g", this->warm_white);
  out.append(buffer);
  out.append("\n");

  out.append("  effect: ");
  out.append("'").append(this->effect).append("'");
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void LightCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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

  out.append("  has_color_mode: ");
  out.append(YESNO(this->has_color_mode));
  out.append("\n");

  out.append("  color_mode: ");
  out.append(proto_enum_to_string<enums::ColorMode>(this->color_mode));
  out.append("\n");

  out.append("  has_color_brightness: ");
  out.append(YESNO(this->has_color_brightness));
  out.append("\n");

  out.append("  color_brightness: ");
  sprintf(buffer, "%g", this->color_brightness);
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

  out.append("  has_cold_white: ");
  out.append(YESNO(this->has_cold_white));
  out.append("\n");

  out.append("  cold_white: ");
  sprintf(buffer, "%g", this->cold_white);
  out.append(buffer);
  out.append("\n");

  out.append("  has_warm_white: ");
  out.append(YESNO(this->has_warm_white));
  out.append("\n");

  out.append("  warm_white: ");
  sprintf(buffer, "%g", this->warm_white);
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
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesSensorResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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

  out.append("  device_class: ");
  out.append("'").append(this->device_class).append("'");
  out.append("\n");

  out.append("  state_class: ");
  out.append(proto_enum_to_string<enums::SensorStateClass>(this->state_class));
  out.append("\n");

  out.append("  legacy_last_reset_type: ");
  out.append(proto_enum_to_string<enums::SensorLastResetType>(this->legacy_last_reset_type));
  out.append("\n");

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void SensorStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SensorStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  sprintf(buffer, "%g", this->state);
  out.append(buffer);
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");
  out.append("}");
}
#endif
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
  buffer.encode_bool(7, this->disabled_by_default);
  buffer.encode_enum<enums::EntityCategory>(8, this->entity_category);
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesSwitchResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void SwitchStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void SwitchCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesTextSensorResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void TextSensorStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("TextSensorStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append("'").append(this->state).append("'");
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");
  out.append("}");
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void SubscribeLogsRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SubscribeLogsRequest {\n");
  out.append("  level: ");
  out.append(proto_enum_to_string<enums::LogLevel>(this->level));
  out.append("\n");

  out.append("  dump_config: ");
  out.append(YESNO(this->dump_config));
  out.append("\n");
  out.append("}");
}
#endif
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
  buffer.encode_string(3, this->message);
  buffer.encode_bool(4, this->send_failed);
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void SubscribeLogsResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SubscribeLogsResponse {\n");
  out.append("  level: ");
  out.append(proto_enum_to_string<enums::LogLevel>(this->level));
  out.append("\n");

  out.append("  message: ");
  out.append("'").append(this->message).append("'");
  out.append("\n");

  out.append("  send_failed: ");
  out.append(YESNO(this->send_failed));
  out.append("\n");
  out.append("}");
}
#endif
void SubscribeHomeassistantServicesRequest::encode(ProtoWriteBuffer buffer) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
void SubscribeHomeassistantServicesRequest::dump_to(std::string &out) const {
  out.append("SubscribeHomeassistantServicesRequest {}");
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void HomeassistantServiceMap::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("HomeassistantServiceMap {\n");
  out.append("  key: ");
  out.append("'").append(this->key).append("'");
  out.append("\n");

  out.append("  value: ");
  out.append("'").append(this->value).append("'");
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void HomeassistantServiceResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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
#endif
void SubscribeHomeAssistantStatesRequest::encode(ProtoWriteBuffer buffer) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
void SubscribeHomeAssistantStatesRequest::dump_to(std::string &out) const {
  out.append("SubscribeHomeAssistantStatesRequest {}");
}
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void SubscribeHomeAssistantStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SubscribeHomeAssistantStateResponse {\n");
  out.append("  entity_id: ");
  out.append("'").append(this->entity_id).append("'");
  out.append("\n");

  out.append("  attribute: ");
  out.append("'").append(this->attribute).append("'");
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void HomeAssistantStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("HomeAssistantStateResponse {\n");
  out.append("  entity_id: ");
  out.append("'").append(this->entity_id).append("'");
  out.append("\n");

  out.append("  state: ");
  out.append("'").append(this->state).append("'");
  out.append("\n");

  out.append("  attribute: ");
  out.append("'").append(this->attribute).append("'");
  out.append("\n");
  out.append("}");
}
#endif
void GetTimeRequest::encode(ProtoWriteBuffer buffer) const {}
#ifdef HAS_PROTO_MESSAGE_DUMP
void GetTimeRequest::dump_to(std::string &out) const { out.append("GetTimeRequest {}"); }
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void GetTimeResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("GetTimeResponse {\n");
  out.append("  epoch_seconds: ");
  sprintf(buffer, "%u", this->epoch_seconds);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesServicesArgument::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesServicesArgument {\n");
  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  type: ");
  out.append(proto_enum_to_string<enums::ServiceArgType>(this->type));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesServicesResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void ExecuteServiceArgument::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void ExecuteServiceRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesCameraResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  icon: ");
  out.append("'").append(this->icon).append("'");
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void CameraImageResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void CameraImageRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("CameraImageRequest {\n");
  out.append("  single: ");
  out.append(YESNO(this->single));
  out.append("\n");

  out.append("  stream: ");
  out.append(YESNO(this->stream));
  out.append("\n");
  out.append("}");
}
#endif
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
    buffer.encode_enum<enums::ClimateMode>(7, it, true);
  }
  buffer.encode_float(8, this->visual_min_temperature);
  buffer.encode_float(9, this->visual_max_temperature);
  buffer.encode_float(10, this->visual_temperature_step);
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesClimateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
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
    out.append(proto_enum_to_string<enums::ClimateMode>(it));
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

  out.append("  legacy_supports_away: ");
  out.append(YESNO(this->legacy_supports_away));
  out.append("\n");

  out.append("  supports_action: ");
  out.append(YESNO(this->supports_action));
  out.append("\n");

  for (const auto &it : this->supported_fan_modes) {
    out.append("  supported_fan_modes: ");
    out.append(proto_enum_to_string<enums::ClimateFanMode>(it));
    out.append("\n");
  }

  for (const auto &it : this->supported_swing_modes) {
    out.append("  supported_swing_modes: ");
    out.append(proto_enum_to_string<enums::ClimateSwingMode>(it));
    out.append("\n");
  }

  for (const auto &it : this->supported_custom_fan_modes) {
    out.append("  supported_custom_fan_modes: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }

  for (const auto &it : this->supported_presets) {
    out.append("  supported_presets: ");
    out.append(proto_enum_to_string<enums::ClimatePreset>(it));
    out.append("\n");
  }

  for (const auto &it : this->supported_custom_presets) {
    out.append("  supported_custom_presets: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  icon: ");
  out.append("'").append(this->icon).append("'");
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");
  out.append("}");
}
#endif
bool ClimateStateResponse::decode_varint(uint32_t field_id, ProtoVarInt value) {
  switch (field_id) {
    case 2: {
      this->mode = value.as_enum<enums::ClimateMode>();
      return true;
    }
    case 7: {
      this->legacy_away = value.as_bool();
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
  buffer.encode_bool(7, this->legacy_away);
  buffer.encode_enum<enums::ClimateAction>(8, this->action);
  buffer.encode_enum<enums::ClimateFanMode>(9, this->fan_mode);
  buffer.encode_enum<enums::ClimateSwingMode>(10, this->swing_mode);
  buffer.encode_string(11, this->custom_fan_mode);
  buffer.encode_enum<enums::ClimatePreset>(12, this->preset);
  buffer.encode_string(13, this->custom_preset);
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ClimateStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ClimateStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  mode: ");
  out.append(proto_enum_to_string<enums::ClimateMode>(this->mode));
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

  out.append("  legacy_away: ");
  out.append(YESNO(this->legacy_away));
  out.append("\n");

  out.append("  action: ");
  out.append(proto_enum_to_string<enums::ClimateAction>(this->action));
  out.append("\n");

  out.append("  fan_mode: ");
  out.append(proto_enum_to_string<enums::ClimateFanMode>(this->fan_mode));
  out.append("\n");

  out.append("  swing_mode: ");
  out.append(proto_enum_to_string<enums::ClimateSwingMode>(this->swing_mode));
  out.append("\n");

  out.append("  custom_fan_mode: ");
  out.append("'").append(this->custom_fan_mode).append("'");
  out.append("\n");

  out.append("  preset: ");
  out.append(proto_enum_to_string<enums::ClimatePreset>(this->preset));
  out.append("\n");

  out.append("  custom_preset: ");
  out.append("'").append(this->custom_preset).append("'");
  out.append("\n");
  out.append("}");
}
#endif
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
      this->has_legacy_away = value.as_bool();
      return true;
    }
    case 11: {
      this->legacy_away = value.as_bool();
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
  buffer.encode_bool(10, this->has_legacy_away);
  buffer.encode_bool(11, this->legacy_away);
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ClimateCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ClimateCommandRequest {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  has_mode: ");
  out.append(YESNO(this->has_mode));
  out.append("\n");

  out.append("  mode: ");
  out.append(proto_enum_to_string<enums::ClimateMode>(this->mode));
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

  out.append("  has_legacy_away: ");
  out.append(YESNO(this->has_legacy_away));
  out.append("\n");

  out.append("  legacy_away: ");
  out.append(YESNO(this->legacy_away));
  out.append("\n");

  out.append("  has_fan_mode: ");
  out.append(YESNO(this->has_fan_mode));
  out.append("\n");

  out.append("  fan_mode: ");
  out.append(proto_enum_to_string<enums::ClimateFanMode>(this->fan_mode));
  out.append("\n");

  out.append("  has_swing_mode: ");
  out.append(YESNO(this->has_swing_mode));
  out.append("\n");

  out.append("  swing_mode: ");
  out.append(proto_enum_to_string<enums::ClimateSwingMode>(this->swing_mode));
  out.append("\n");

  out.append("  has_custom_fan_mode: ");
  out.append(YESNO(this->has_custom_fan_mode));
  out.append("\n");

  out.append("  custom_fan_mode: ");
  out.append("'").append(this->custom_fan_mode).append("'");
  out.append("\n");

  out.append("  has_preset: ");
  out.append(YESNO(this->has_preset));
  out.append("\n");

  out.append("  preset: ");
  out.append(proto_enum_to_string<enums::ClimatePreset>(this->preset));
  out.append("\n");

  out.append("  has_custom_preset: ");
  out.append(YESNO(this->has_custom_preset));
  out.append("\n");

  out.append("  custom_preset: ");
  out.append("'").append(this->custom_preset).append("'");
  out.append("\n");
  out.append("}");
}
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesNumberResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesNumberResponse {\n");
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

  out.append("  min_value: ");
  sprintf(buffer, "%g", this->min_value);
  out.append(buffer);
  out.append("\n");

  out.append("  max_value: ");
  sprintf(buffer, "%g", this->max_value);
  out.append(buffer);
  out.append("\n");

  out.append("  step: ");
  sprintf(buffer, "%g", this->step);
  out.append(buffer);
  out.append("\n");

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");

  out.append("  unit_of_measurement: ");
  out.append("'").append(this->unit_of_measurement).append("'");
  out.append("\n");

  out.append("  mode: ");
  out.append(proto_enum_to_string<enums::NumberMode>(this->mode));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void NumberStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("NumberStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  sprintf(buffer, "%g", this->state);
  out.append(buffer);
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void NumberCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("NumberCommandRequest {\n");
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
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesSelectResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesSelectResponse {\n");
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

  for (const auto &it : this->options) {
    out.append("  options: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void SelectStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SelectStateResponse {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append("'").append(this->state).append("'");
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void SelectCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SelectCommandRequest {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append("'").append(this->state).append("'");
  out.append("\n");
  out.append("}");
}
#endif
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
}
#ifdef HAS_PROTO_MESSAGE_DUMP
void ListEntitiesButtonResponse::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ListEntitiesButtonResponse {\n");
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

  out.append("  disabled_by_default: ");
  out.append(YESNO(this->disabled_by_default));
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");

  out.append("  device_class: ");
  out.append("'").append(this->device_class).append("'");
  out.append("\n");
  out.append("}");
}
#endif
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
#ifdef HAS_PROTO_MESSAGE_DUMP
void ButtonCommandRequest::dump_to(std::string &out) const {
  char buffer[64];
  out.append("ButtonCommandRequest {\n");
  out.append("  key: ");
  sprintf(buffer, "%u", this->key);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif

}  // namespace api
}  // namespace esphome
