#pragma once

#include "proto.h"

namespace esphome {
namespace api {

enum EnumLegacyCoverState : uint32_t {
  LEGACY_COVER_STATE_OPEN = 0,
  LEGACY_COVER_STATE_CLOSED = 1,
};
enum EnumCoverOperation : uint32_t {
  COVER_OPERATION_IDLE = 0,
  COVER_OPERATION_IS_OPENING = 1,
  COVER_OPERATION_IS_CLOSING = 2,
};
enum EnumLegacyCoverCommand : uint32_t {
  LEGACY_COVER_COMMAND_OPEN = 0,
  LEGACY_COVER_COMMAND_CLOSE = 1,
  LEGACY_COVER_COMMAND_STOP = 2,
};
enum EnumFanSpeed : uint32_t {
  FAN_SPEED_LOW = 0,
  FAN_SPEED_MEDIUM = 1,
  FAN_SPEED_HIGH = 2,
};
enum EnumLogLevel : uint32_t {
  LOG_LEVEL_NONE = 0,
  LOG_LEVEL_ERROR = 1,
  LOG_LEVEL_WARN = 2,
  LOG_LEVEL_INFO = 3,
  LOG_LEVEL_DEBUG = 4,
  LOG_LEVEL_VERBOSE = 5,
  LOG_LEVEL_VERY_VERBOSE = 6,
};
enum EnumServiceArgType : uint32_t {
  SERVICE_ARG_TYPE_BOOL = 0,
  SERVICE_ARG_TYPE_INT = 1,
  SERVICE_ARG_TYPE_FLOAT = 2,
  SERVICE_ARG_TYPE_STRING = 3,
  SERVICE_ARG_TYPE_BOOL_ARRAY = 4,
  SERVICE_ARG_TYPE_INT_ARRAY = 5,
  SERVICE_ARG_TYPE_FLOAT_ARRAY = 6,
  SERVICE_ARG_TYPE_STRING_ARRAY = 7,
};
enum EnumClimateMode : uint32_t {
  CLIMATE_MODE_OFF = 0,
  CLIMATE_MODE_AUTO = 1,
  CLIMATE_MODE_COOL = 2,
  CLIMATE_MODE_HEAT = 3,
};
enum EnumClimateAction : uint32_t {
  CLIMATE_ACTION_OFF = 0,
  CLIMATE_ACTION_COOLING = 2,
  CLIMATE_ACTION_HEATING = 3,
};
class HelloRequest : public ProtoMessage {
 public:
  std::string client_info{};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class HelloResponse : public ProtoMessage {
 public:
  uint32_t api_version_major{0};  // NOLINT
  uint32_t api_version_minor{0};  // NOLINT
  std::string server_info{};      // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ConnectRequest : public ProtoMessage {
 public:
  std::string password{};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class ConnectResponse : public ProtoMessage {
 public:
  bool invalid_password{false};  // NOLINT
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
  bool uses_password{false};       // NOLINT
  std::string name{};              // NOLINT
  std::string mac_address{};       // NOLINT
  std::string esphome_version{};   // NOLINT
  std::string compilation_time{};  // NOLINT
  std::string model{};             // NOLINT
  bool has_deep_sleep{false};      // NOLINT
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
  std::string object_id{};              // NOLINT
  uint32_t key{0};                      // NOLINT
  std::string name{};                   // NOLINT
  std::string unique_id{};              // NOLINT
  std::string device_class{};           // NOLINT
  bool is_status_binary_sensor{false};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BinarySensorStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};    // NOLINT
  bool state{false};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesCoverResponse : public ProtoMessage {
 public:
  std::string object_id{};        // NOLINT
  uint32_t key{0};                // NOLINT
  std::string name{};             // NOLINT
  std::string unique_id{};        // NOLINT
  bool assumed_state{false};      // NOLINT
  bool supports_position{false};  // NOLINT
  bool supports_tilt{false};      // NOLINT
  std::string device_class{};     // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class CoverStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};                         // NOLINT
  EnumLegacyCoverState legacy_state{};     // NOLINT
  float position{0.0f};                    // NOLINT
  float tilt{0.0f};                        // NOLINT
  EnumCoverOperation current_operation{};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class CoverCommandRequest : public ProtoMessage {
 public:
  uint32_t key{0};                          // NOLINT
  bool has_legacy_command{false};           // NOLINT
  EnumLegacyCoverCommand legacy_command{};  // NOLINT
  bool has_position{false};                 // NOLINT
  float position{0.0f};                     // NOLINT
  bool has_tilt{false};                     // NOLINT
  float tilt{0.0f};                         // NOLINT
  bool stop{false};                         // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesFanResponse : public ProtoMessage {
 public:
  std::string object_id{};           // NOLINT
  uint32_t key{0};                   // NOLINT
  std::string name{};                // NOLINT
  std::string unique_id{};           // NOLINT
  bool supports_oscillation{false};  // NOLINT
  bool supports_speed{false};        // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class FanStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};          // NOLINT
  bool state{false};        // NOLINT
  bool oscillating{false};  // NOLINT
  EnumFanSpeed speed{};     // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class FanCommandRequest : public ProtoMessage {
 public:
  uint32_t key{0};              // NOLINT
  bool has_state{false};        // NOLINT
  bool state{false};            // NOLINT
  bool has_speed{false};        // NOLINT
  EnumFanSpeed speed{};         // NOLINT
  bool has_oscillating{false};  // NOLINT
  bool oscillating{false};      // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesLightResponse : public ProtoMessage {
 public:
  std::string object_id{};                 // NOLINT
  uint32_t key{0};                         // NOLINT
  std::string name{};                      // NOLINT
  std::string unique_id{};                 // NOLINT
  bool supports_brightness{false};         // NOLINT
  bool supports_rgb{false};                // NOLINT
  bool supports_white_value{false};        // NOLINT
  bool supports_color_temperature{false};  // NOLINT
  float min_mireds{0.0f};                  // NOLINT
  float max_mireds{0.0f};                  // NOLINT
  std::vector<std::string> effects{};      // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class LightStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};                // NOLINT
  bool state{false};              // NOLINT
  float brightness{0.0f};         // NOLINT
  float red{0.0f};                // NOLINT
  float green{0.0f};              // NOLINT
  float blue{0.0f};               // NOLINT
  float white{0.0f};              // NOLINT
  float color_temperature{0.0f};  // NOLINT
  std::string effect{};           // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class LightCommandRequest : public ProtoMessage {
 public:
  uint32_t key{0};                    // NOLINT
  bool has_state{false};              // NOLINT
  bool state{false};                  // NOLINT
  bool has_brightness{false};         // NOLINT
  float brightness{0.0f};             // NOLINT
  bool has_rgb{false};                // NOLINT
  float red{0.0f};                    // NOLINT
  float green{0.0f};                  // NOLINT
  float blue{0.0f};                   // NOLINT
  bool has_white{false};              // NOLINT
  float white{0.0f};                  // NOLINT
  bool has_color_temperature{false};  // NOLINT
  float color_temperature{0.0f};      // NOLINT
  bool has_transition_length{false};  // NOLINT
  uint32_t transition_length{0};      // NOLINT
  bool has_flash_length{false};       // NOLINT
  uint32_t flash_length{0};           // NOLINT
  bool has_effect{false};             // NOLINT
  std::string effect{};               // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesSensorResponse : public ProtoMessage {
 public:
  std::string object_id{};            // NOLINT
  uint32_t key{0};                    // NOLINT
  std::string name{};                 // NOLINT
  std::string unique_id{};            // NOLINT
  std::string icon{};                 // NOLINT
  std::string unit_of_measurement{};  // NOLINT
  int32_t accuracy_decimals{0};       // NOLINT
  bool force_update{false};           // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SensorStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};    // NOLINT
  float state{0.0f};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
};
class ListEntitiesSwitchResponse : public ProtoMessage {
 public:
  std::string object_id{};    // NOLINT
  uint32_t key{0};            // NOLINT
  std::string name{};         // NOLINT
  std::string unique_id{};    // NOLINT
  std::string icon{};         // NOLINT
  bool assumed_state{false};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SwitchStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};    // NOLINT
  bool state{false};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SwitchCommandRequest : public ProtoMessage {
 public:
  uint32_t key{0};    // NOLINT
  bool state{false};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesTextSensorResponse : public ProtoMessage {
 public:
  std::string object_id{};  // NOLINT
  uint32_t key{0};          // NOLINT
  std::string name{};       // NOLINT
  std::string unique_id{};  // NOLINT
  std::string icon{};       // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class TextSensorStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};      // NOLINT
  std::string state{};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class SubscribeLogsRequest : public ProtoMessage {
 public:
  EnumLogLevel level{};     // NOLINT
  bool dump_config{false};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SubscribeLogsResponse : public ProtoMessage {
 public:
  EnumLogLevel level{};     // NOLINT
  std::string tag{};        // NOLINT
  std::string message{};    // NOLINT
  bool send_failed{false};  // NOLINT
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
  std::string key{};    // NOLINT
  std::string value{};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class HomeassistantServiceResponse : public ProtoMessage {
 public:
  std::string service{};                                 // NOLINT
  std::vector<HomeassistantServiceMap> data{};           // NOLINT
  std::vector<HomeassistantServiceMap> data_template{};  // NOLINT
  std::vector<HomeassistantServiceMap> variables{};      // NOLINT
  bool is_event{false};                                  // NOLINT
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
  std::string entity_id{};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class HomeAssistantStateResponse : public ProtoMessage {
 public:
  std::string entity_id{};  // NOLINT
  std::string state{};      // NOLINT
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
  uint32_t epoch_seconds{0};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
};
class ListEntitiesServicesArgument : public ProtoMessage {
 public:
  std::string name{};         // NOLINT
  EnumServiceArgType type{};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesServicesResponse : public ProtoMessage {
 public:
  std::string name{};                                // NOLINT
  uint32_t key{0};                                   // NOLINT
  std::vector<ListEntitiesServicesArgument> args{};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class ExecuteServiceArgument : public ProtoMessage {
 public:
  bool bool_{false};                        // NOLINT
  int32_t legacy_int{0};                    // NOLINT
  float float_{0.0f};                       // NOLINT
  std::string string_{};                    // NOLINT
  int32_t int_{0};                          // NOLINT
  std::vector<bool> bool_array{};           // NOLINT
  std::vector<int32_t> int_array{};         // NOLINT
  std::vector<float> float_array{};         // NOLINT
  std::vector<std::string> string_array{};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ExecuteServiceRequest : public ProtoMessage {
 public:
  uint32_t key{0};                             // NOLINT
  std::vector<ExecuteServiceArgument> args{};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class ListEntitiesCameraResponse : public ProtoMessage {
 public:
  std::string object_id{};  // NOLINT
  uint32_t key{0};          // NOLINT
  std::string name{};       // NOLINT
  std::string unique_id{};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class CameraImageResponse : public ProtoMessage {
 public:
  uint32_t key{0};     // NOLINT
  std::string data{};  // NOLINT
  bool done{false};    // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class CameraImageRequest : public ProtoMessage {
 public:
  bool single{false};  // NOLINT
  bool stream{false};  // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesClimateResponse : public ProtoMessage {
 public:
  std::string object_id{};                            // NOLINT
  uint32_t key{0};                                    // NOLINT
  std::string name{};                                 // NOLINT
  std::string unique_id{};                            // NOLINT
  bool supports_current_temperature{false};           // NOLINT
  bool supports_two_point_target_temperature{false};  // NOLINT
  std::vector<EnumClimateMode> supported_modes{};     // NOLINT
  float visual_min_temperature{0.0f};                 // NOLINT
  float visual_max_temperature{0.0f};                 // NOLINT
  float visual_temperature_step{0.0f};                // NOLINT
  bool supports_away{false};                          // NOLINT
  bool supports_action{false};                        // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ClimateStateResponse : public ProtoMessage {
 public:
  uint32_t key{0};                      // NOLINT
  EnumClimateMode mode{};               // NOLINT
  float current_temperature{0.0f};      // NOLINT
  float target_temperature{0.0f};       // NOLINT
  float target_temperature_low{0.0f};   // NOLINT
  float target_temperature_high{0.0f};  // NOLINT
  bool away{false};                     // NOLINT
  EnumClimateAction action{};           // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ClimateCommandRequest : public ProtoMessage {
 public:
  uint32_t key{0};                          // NOLINT
  bool has_mode{false};                     // NOLINT
  EnumClimateMode mode{};                   // NOLINT
  bool has_target_temperature{false};       // NOLINT
  float target_temperature{0.0f};           // NOLINT
  bool has_target_temperature_low{false};   // NOLINT
  float target_temperature_low{0.0f};       // NOLINT
  bool has_target_temperature_high{false};  // NOLINT
  float target_temperature_high{0.0f};      // NOLINT
  bool has_away{false};                     // NOLINT
  bool away{false};                         // NOLINT
  void encode(ProtoWriteBuffer buffer) const override;
  void dump_to(std::string &out) const override;

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};

}  // namespace api
}  // namespace esphome
