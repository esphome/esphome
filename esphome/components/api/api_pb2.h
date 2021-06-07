// This file was automatically generated with a tool.
// See scripts/api_protobuf/api_protobuf.py
#pragma once

#include "proto.h"

namespace esphome {
namespace api {

namespace enums {

enum LegacyCoverState : uint32_t {
  LEGACY_COVER_STATE_OPEN = 0,
  LEGACY_COVER_STATE_CLOSED = 1,
};
enum CoverOperation : uint32_t {
  COVER_OPERATION_IDLE = 0,
  COVER_OPERATION_IS_OPENING = 1,
  COVER_OPERATION_IS_CLOSING = 2,
};
enum LegacyCoverCommand : uint32_t {
  LEGACY_COVER_COMMAND_OPEN = 0,
  LEGACY_COVER_COMMAND_CLOSE = 1,
  LEGACY_COVER_COMMAND_STOP = 2,
};
enum FanSpeed : uint32_t {
  FAN_SPEED_LOW = 0,
  FAN_SPEED_MEDIUM = 1,
  FAN_SPEED_HIGH = 2,
};
enum FanDirection : uint32_t {
  FAN_DIRECTION_FORWARD = 0,
  FAN_DIRECTION_REVERSE = 1,
};
enum SensorStateClass : uint32_t {
  STATE_CLASS_NONE = 0,
  STATE_CLASS_MEASUREMENT = 1,
};
enum LogLevel : uint32_t {
  LOG_LEVEL_NONE = 0,
  LOG_LEVEL_ERROR = 1,
  LOG_LEVEL_WARN = 2,
  LOG_LEVEL_INFO = 3,
  LOG_LEVEL_DEBUG = 4,
  LOG_LEVEL_VERBOSE = 5,
  LOG_LEVEL_VERY_VERBOSE = 6,
};
enum ServiceArgType : uint32_t {
  SERVICE_ARG_TYPE_BOOL = 0,
  SERVICE_ARG_TYPE_INT = 1,
  SERVICE_ARG_TYPE_FLOAT = 2,
  SERVICE_ARG_TYPE_STRING = 3,
  SERVICE_ARG_TYPE_BOOL_ARRAY = 4,
  SERVICE_ARG_TYPE_INT_ARRAY = 5,
  SERVICE_ARG_TYPE_FLOAT_ARRAY = 6,
  SERVICE_ARG_TYPE_STRING_ARRAY = 7,
};
enum ClimateMode : uint32_t {
  CLIMATE_MODE_OFF = 0,
  CLIMATE_MODE_HEAT_COOL = 1,
  CLIMATE_MODE_COOL = 2,
  CLIMATE_MODE_HEAT = 3,
  CLIMATE_MODE_FAN_ONLY = 4,
  CLIMATE_MODE_DRY = 5,
  CLIMATE_MODE_AUTO = 6,
};
enum ClimateFanMode : uint32_t {
  CLIMATE_FAN_ON = 0,
  CLIMATE_FAN_OFF = 1,
  CLIMATE_FAN_AUTO = 2,
  CLIMATE_FAN_LOW = 3,
  CLIMATE_FAN_MEDIUM = 4,
  CLIMATE_FAN_HIGH = 5,
  CLIMATE_FAN_MIDDLE = 6,
  CLIMATE_FAN_FOCUS = 7,
  CLIMATE_FAN_DIFFUSE = 8,
};
enum ClimateSwingMode : uint32_t {
  CLIMATE_SWING_OFF = 0,
  CLIMATE_SWING_BOTH = 1,
  CLIMATE_SWING_VERTICAL = 2,
  CLIMATE_SWING_HORIZONTAL = 3,
};
enum ClimateAction : uint32_t {
  CLIMATE_ACTION_OFF = 0,
  CLIMATE_ACTION_COOLING = 2,
  CLIMATE_ACTION_HEATING = 3,
  CLIMATE_ACTION_IDLE = 4,
  CLIMATE_ACTION_DRYING = 5,
  CLIMATE_ACTION_FAN = 6,
};
enum ClimatePreset : uint32_t {
  CLIMATE_PRESET_ECO = 0,
  CLIMATE_PRESET_AWAY = 1,
  CLIMATE_PRESET_BOOST = 2,
  CLIMATE_PRESET_COMFORT = 3,
  CLIMATE_PRESET_HOME = 4,
  CLIMATE_PRESET_SLEEP = 5,
  CLIMATE_PRESET_ACTIVITY = 6,
};

}  // namespace enums

class HelloRequest : public ProtoMessage {
 public:
  std::string client_info{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class HelloResponse : public ProtoMessage {
 public:
  uint32_t api_version_major{0};
  uint32_t api_version_minor{0};
  std::string server_info{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ConnectRequest : public ProtoMessage {
 public:
  std::string password{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class ConnectResponse : public ProtoMessage {
 public:
  bool invalid_password{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class DisconnectRequest : public ProtoMessage {
 public:
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
};
class DisconnectResponse : public ProtoMessage {
 public:
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
};
class PingRequest : public ProtoMessage {
 public:
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
};
class PingResponse : public ProtoMessage {
 public:
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
};
class DeviceInfoRequest : public ProtoMessage {
 public:
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
};
class DeviceInfoResponse : public ProtoMessage {
 public:
  bool uses_password{false};
  std::string name{};
  std::string mac_address{};
  std::string esphome_version{};
  std::string compilation_time{};
  std::string model{};
  bool has_deep_sleep{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesRequest : public ProtoMessage {
 public:
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
};
class ListEntitiesDoneResponse : public ProtoMessage {
 public:
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
};
class SubscribeStatesRequest : public ProtoMessage {
 public:
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
};
class ListEntitiesBinarySensorResponse : public ProtoMessage {
 public:
  std::string object_id{};
  uint32_t key{0};
  std::string name{};
  std::string unique_id{};
  std::string device_class{};
  bool is_status_binary_sensor{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BinarySensorStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};
  bool state{false};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesCoverResponse : public ProtoMessage {
 public:
  std::string object_id{};
  uint32_t key{0};
  std::string name{};
  std::string unique_id{};
  bool assumed_state{false};
  bool supports_position{false};
  bool supports_tilt{false};
  std::string device_class{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class CoverStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};
  enums::LegacyCoverState legacy_state{};
  float position{0.0f};
  float tilt{0.0f};
  enums::CoverOperation current_operation{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class CoverCommandRequest : public ProtoMessage {
 public:
  uint32_t key{0};
  bool has_legacy_command{false};
  enums::LegacyCoverCommand legacy_command{};
  bool has_position{false};
  float position{0.0f};
  bool has_tilt{false};
  float tilt{0.0f};
  bool stop{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesFanResponse : public ProtoMessage {
 public:
  std::string object_id{};
  uint32_t key{0};
  std::string name{};
  std::string unique_id{};
  bool supports_oscillation{false};
  bool supports_speed{false};
  bool supports_direction{false};
  int32_t supported_speed_count{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class FanStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};
  bool state{false};
  bool oscillating{false};
  enums::FanSpeed speed{};
  enums::FanDirection direction{};
  int32_t speed_level{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class FanCommandRequest : public ProtoMessage {
 public:
  uint32_t key{0};
  bool has_state{false};
  bool state{false};
  bool has_speed{false};
  enums::FanSpeed speed{};
  bool has_oscillating{false};
  bool oscillating{false};
  bool has_direction{false};
  enums::FanDirection direction{};
  bool has_speed_level{false};
  int32_t speed_level{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesLightResponse : public ProtoMessage {
 public:
  std::string object_id{};
  uint32_t key{0};
  std::string name{};
  std::string unique_id{};
  bool supports_brightness{false};
  bool supports_rgb{false};
  bool supports_white_value{false};
  bool supports_color_temperature{false};
  float min_mireds{0.0f};
  float max_mireds{0.0f};
  std::vector<std::string> effects{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class LightStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};
  bool state{false};
  float brightness{0.0f};
  float red{0.0f};
  float green{0.0f};
  float blue{0.0f};
  float white{0.0f};
  float color_temperature{0.0f};
  std::string effect{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class LightCommandRequest : public ProtoMessage {
 public:
  uint32_t key{0};
  bool has_state{false};
  bool state{false};
  bool has_brightness{false};
  float brightness{0.0f};
  bool has_rgb{false};
  float red{0.0f};
  float green{0.0f};
  float blue{0.0f};
  bool has_white{false};
  float white{0.0f};
  bool has_color_temperature{false};
  float color_temperature{0.0f};
  bool has_transition_length{false};
  uint32_t transition_length{0};
  bool has_flash_length{false};
  uint32_t flash_length{0};
  bool has_effect{false};
  std::string effect{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesSensorResponse : public ProtoMessage {
 public:
  std::string object_id{};
  uint32_t key{0};
  std::string name{};
  std::string unique_id{};
  std::string icon{};
  std::string unit_of_measurement{};
  int32_t accuracy_decimals{0};
  bool force_update{false};
  std::string device_class{};
  enums::SensorStateClass state_class{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SensorStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};
  float state{0.0f};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesSwitchResponse : public ProtoMessage {
 public:
  std::string object_id{};
  uint32_t key{0};
  std::string name{};
  std::string unique_id{};
  std::string icon{};
  bool assumed_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SwitchStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};
  bool state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SwitchCommandRequest : public ProtoMessage {
 public:
  uint32_t key{0};
  bool state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesTextSensorResponse : public ProtoMessage {
 public:
  std::string object_id{};
  uint32_t key{0};
  std::string name{};
  std::string unique_id{};
  std::string icon{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class TextSensorStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};
  std::string state{};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SubscribeLogsRequest : public ProtoMessage {
 public:
  enums::LogLevel level{};
  bool dump_config{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SubscribeLogsResponse : public ProtoMessage {
 public:
  enums::LogLevel level{};
  std::string tag{};
  std::string message{};
  bool send_failed{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SubscribeHomeassistantServicesRequest : public ProtoMessage {
 public:
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
};
class HomeassistantServiceMap : public ProtoMessage {
 public:
  std::string key{};
  std::string value{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class HomeassistantServiceResponse : public ProtoMessage {
 public:
  std::string service{};
  std::vector<HomeassistantServiceMap> data{};
  std::vector<HomeassistantServiceMap> data_template{};
  std::vector<HomeassistantServiceMap> variables{};
  bool is_event{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SubscribeHomeAssistantStatesRequest : public ProtoMessage {
 public:
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
};
class SubscribeHomeAssistantStateResponse : public ProtoMessage {
 public:
  std::string entity_id{};
  std::string attribute{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class HomeAssistantStateResponse : public ProtoMessage {
 public:
  std::string entity_id{};
  std::string state{};
  std::string attribute{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class GetTimeRequest : public ProtoMessage {
 public:
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
};
class GetTimeResponse : public ProtoMessage {
 public:
  uint32_t epoch_seconds{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
};
class ListEntitiesServicesArgument : public ProtoMessage {
 public:
  std::string name{};
  enums::ServiceArgType type{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesServicesResponse : public ProtoMessage {
 public:
  std::string name{};
  uint32_t key{0};
  std::vector<ListEntitiesServicesArgument> args{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class ExecuteServiceArgument : public ProtoMessage {
 public:
  bool bool_{false};
  int32_t legacy_int{0};
  float float_{0.0f};
  std::string string_{};
  int32_t int_{0};
  std::vector<bool> bool_array{};
  std::vector<int32_t> int_array{};
  std::vector<float> float_array{};
  std::vector<std::string> string_array{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ExecuteServiceRequest : public ProtoMessage {
 public:
  uint32_t key{0};
  std::vector<ExecuteServiceArgument> args{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class ListEntitiesCameraResponse : public ProtoMessage {
 public:
  std::string object_id{};
  uint32_t key{0};
  std::string name{};
  std::string unique_id{};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class CameraImageResponse : public ProtoMessage {
 public:
  uint32_t key{0};
  std::string data{};
  bool done{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class CameraImageRequest : public ProtoMessage {
 public:
  bool single{false};
  bool stream{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesClimateResponse : public ProtoMessage {
 public:
  std::string object_id{};                                       // NOLINT
  uint32_t key{0};                                               // NOLINT
  std::string name{};                                            // NOLINT
  std::string unique_id{};                                       // NOLINT
  bool supports_current_temperature{false};                      // NOLINT
  bool supports_two_point_target_temperature{false};             // NOLINT
  std::vector<enums::ClimateMode> supported_modes{};             // NOLINT
  float visual_min_temperature{0.0f};                            // NOLINT
  float visual_max_temperature{0.0f};                            // NOLINT
  float visual_temperature_step{0.0f};                           // NOLINT
  bool supports_away{false};                                     // NOLINT
  bool supports_action{false};                                   // NOLINT
  std::vector<enums::ClimateFanMode> supported_fan_modes{};      // NOLINT
  std::vector<enums::ClimateSwingMode> supported_swing_modes{};  // NOLINT
  std::vector<std::string> supported_custom_fan_modes{};         // NOLINT
  std::vector<enums::ClimatePreset> supported_presets{};         // NOLINT
  std::vector<std::string> supported_custom_presets{};           // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ClimateStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};                       // NOLINT
  enums::ClimateMode mode{};             // NOLINT
  float current_temperature{0.0f};       // NOLINT
  float target_temperature{0.0f};        // NOLINT
  float target_temperature_low{0.0f};    // NOLINT
  float target_temperature_high{0.0f};   // NOLINT
  bool away{false};                      // NOLINT
  enums::ClimateAction action{};         // NOLINT
  enums::ClimateFanMode fan_mode{};      // NOLINT
  enums::ClimateSwingMode swing_mode{};  // NOLINT
  std::string custom_fan_mode{};         // NOLINT
  enums::ClimatePreset preset{};         // NOLINT
  std::string custom_preset{};           // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ClimateCommandRequest : public ProtoMessage {
 public:
  uint32_t key{0};                          // NOLINT
  bool has_mode{false};                     // NOLINT
  enums::ClimateMode mode{};                // NOLINT
  bool has_target_temperature{false};       // NOLINT
  float target_temperature{0.0f};           // NOLINT
  bool has_target_temperature_low{false};   // NOLINT
  float target_temperature_low{0.0f};       // NOLINT
  bool has_target_temperature_high{false};  // NOLINT
  float target_temperature_high{0.0f};      // NOLINT
  bool has_away{false};                     // NOLINT
  bool away{false};                         // NOLINT
  bool has_fan_mode{false};                 // NOLINT
  enums::ClimateFanMode fan_mode{};         // NOLINT
  bool has_swing_mode{false};               // NOLINT
  enums::ClimateSwingMode swing_mode{};     // NOLINT
  bool has_custom_fan_mode{false};          // NOLINT
  std::string custom_fan_mode{};            // NOLINT
  bool has_preset{false};                   // NOLINT
  enums::ClimatePreset preset{};            // NOLINT
  bool has_custom_preset{false};            // NOLINT
  std::string custom_preset{};              // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};

}  // namespace api
}  // namespace esphome
