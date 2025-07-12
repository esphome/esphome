// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#pragma once

#include "esphome/core/defines.h"

#include "proto.h"

namespace esphome {
namespace api {

namespace enums {

enum EntityCategory : uint32_t {
  ENTITY_CATEGORY_NONE = 0,
  ENTITY_CATEGORY_CONFIG = 1,
  ENTITY_CATEGORY_DIAGNOSTIC = 2,
};
#ifdef USE_COVER
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
#endif
#ifdef USE_FAN
enum FanSpeed : uint32_t {
  FAN_SPEED_LOW = 0,
  FAN_SPEED_MEDIUM = 1,
  FAN_SPEED_HIGH = 2,
};
enum FanDirection : uint32_t {
  FAN_DIRECTION_FORWARD = 0,
  FAN_DIRECTION_REVERSE = 1,
};
#endif
#ifdef USE_LIGHT
enum ColorMode : uint32_t {
  COLOR_MODE_UNKNOWN = 0,
  COLOR_MODE_ON_OFF = 1,
  COLOR_MODE_LEGACY_BRIGHTNESS = 2,
  COLOR_MODE_BRIGHTNESS = 3,
  COLOR_MODE_WHITE = 7,
  COLOR_MODE_COLOR_TEMPERATURE = 11,
  COLOR_MODE_COLD_WARM_WHITE = 19,
  COLOR_MODE_RGB = 35,
  COLOR_MODE_RGB_WHITE = 39,
  COLOR_MODE_RGB_COLOR_TEMPERATURE = 47,
  COLOR_MODE_RGB_COLD_WARM_WHITE = 51,
};
#endif
#ifdef USE_SENSOR
enum SensorStateClass : uint32_t {
  STATE_CLASS_NONE = 0,
  STATE_CLASS_MEASUREMENT = 1,
  STATE_CLASS_TOTAL_INCREASING = 2,
  STATE_CLASS_TOTAL = 3,
};
enum SensorLastResetType : uint32_t {
  LAST_RESET_NONE = 0,
  LAST_RESET_NEVER = 1,
  LAST_RESET_AUTO = 2,
};
#endif
enum LogLevel : uint32_t {
  LOG_LEVEL_NONE = 0,
  LOG_LEVEL_ERROR = 1,
  LOG_LEVEL_WARN = 2,
  LOG_LEVEL_INFO = 3,
  LOG_LEVEL_CONFIG = 4,
  LOG_LEVEL_DEBUG = 5,
  LOG_LEVEL_VERBOSE = 6,
  LOG_LEVEL_VERY_VERBOSE = 7,
};
#ifdef USE_API_SERVICES
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
#endif
#ifdef USE_CLIMATE
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
  CLIMATE_FAN_QUIET = 9,
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
  CLIMATE_PRESET_NONE = 0,
  CLIMATE_PRESET_HOME = 1,
  CLIMATE_PRESET_AWAY = 2,
  CLIMATE_PRESET_BOOST = 3,
  CLIMATE_PRESET_COMFORT = 4,
  CLIMATE_PRESET_ECO = 5,
  CLIMATE_PRESET_SLEEP = 6,
  CLIMATE_PRESET_ACTIVITY = 7,
};
#endif
#ifdef USE_NUMBER
enum NumberMode : uint32_t {
  NUMBER_MODE_AUTO = 0,
  NUMBER_MODE_BOX = 1,
  NUMBER_MODE_SLIDER = 2,
};
#endif
#ifdef USE_LOCK
enum LockState : uint32_t {
  LOCK_STATE_NONE = 0,
  LOCK_STATE_LOCKED = 1,
  LOCK_STATE_UNLOCKED = 2,
  LOCK_STATE_JAMMED = 3,
  LOCK_STATE_LOCKING = 4,
  LOCK_STATE_UNLOCKING = 5,
};
enum LockCommand : uint32_t {
  LOCK_UNLOCK = 0,
  LOCK_LOCK = 1,
  LOCK_OPEN = 2,
};
#endif
#ifdef USE_MEDIA_PLAYER
enum MediaPlayerState : uint32_t {
  MEDIA_PLAYER_STATE_NONE = 0,
  MEDIA_PLAYER_STATE_IDLE = 1,
  MEDIA_PLAYER_STATE_PLAYING = 2,
  MEDIA_PLAYER_STATE_PAUSED = 3,
};
enum MediaPlayerCommand : uint32_t {
  MEDIA_PLAYER_COMMAND_PLAY = 0,
  MEDIA_PLAYER_COMMAND_PAUSE = 1,
  MEDIA_PLAYER_COMMAND_STOP = 2,
  MEDIA_PLAYER_COMMAND_MUTE = 3,
  MEDIA_PLAYER_COMMAND_UNMUTE = 4,
};
enum MediaPlayerFormatPurpose : uint32_t {
  MEDIA_PLAYER_FORMAT_PURPOSE_DEFAULT = 0,
  MEDIA_PLAYER_FORMAT_PURPOSE_ANNOUNCEMENT = 1,
};
#endif
#ifdef USE_BLUETOOTH_PROXY
enum BluetoothDeviceRequestType : uint32_t {
  BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT = 0,
  BLUETOOTH_DEVICE_REQUEST_TYPE_DISCONNECT = 1,
  BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR = 2,
  BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR = 3,
  BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE = 4,
  BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE = 5,
  BLUETOOTH_DEVICE_REQUEST_TYPE_CLEAR_CACHE = 6,
};
enum BluetoothScannerState : uint32_t {
  BLUETOOTH_SCANNER_STATE_IDLE = 0,
  BLUETOOTH_SCANNER_STATE_STARTING = 1,
  BLUETOOTH_SCANNER_STATE_RUNNING = 2,
  BLUETOOTH_SCANNER_STATE_FAILED = 3,
  BLUETOOTH_SCANNER_STATE_STOPPING = 4,
  BLUETOOTH_SCANNER_STATE_STOPPED = 5,
};
enum BluetoothScannerMode : uint32_t {
  BLUETOOTH_SCANNER_MODE_PASSIVE = 0,
  BLUETOOTH_SCANNER_MODE_ACTIVE = 1,
};
#endif
enum VoiceAssistantSubscribeFlag : uint32_t {
  VOICE_ASSISTANT_SUBSCRIBE_NONE = 0,
  VOICE_ASSISTANT_SUBSCRIBE_API_AUDIO = 1,
};
enum VoiceAssistantRequestFlag : uint32_t {
  VOICE_ASSISTANT_REQUEST_NONE = 0,
  VOICE_ASSISTANT_REQUEST_USE_VAD = 1,
  VOICE_ASSISTANT_REQUEST_USE_WAKE_WORD = 2,
};
#ifdef USE_VOICE_ASSISTANT
enum VoiceAssistantEvent : uint32_t {
  VOICE_ASSISTANT_ERROR = 0,
  VOICE_ASSISTANT_RUN_START = 1,
  VOICE_ASSISTANT_RUN_END = 2,
  VOICE_ASSISTANT_STT_START = 3,
  VOICE_ASSISTANT_STT_END = 4,
  VOICE_ASSISTANT_INTENT_START = 5,
  VOICE_ASSISTANT_INTENT_END = 6,
  VOICE_ASSISTANT_TTS_START = 7,
  VOICE_ASSISTANT_TTS_END = 8,
  VOICE_ASSISTANT_WAKE_WORD_START = 9,
  VOICE_ASSISTANT_WAKE_WORD_END = 10,
  VOICE_ASSISTANT_STT_VAD_START = 11,
  VOICE_ASSISTANT_STT_VAD_END = 12,
  VOICE_ASSISTANT_TTS_STREAM_START = 98,
  VOICE_ASSISTANT_TTS_STREAM_END = 99,
  VOICE_ASSISTANT_INTENT_PROGRESS = 100,
};
enum VoiceAssistantTimerEvent : uint32_t {
  VOICE_ASSISTANT_TIMER_STARTED = 0,
  VOICE_ASSISTANT_TIMER_UPDATED = 1,
  VOICE_ASSISTANT_TIMER_CANCELLED = 2,
  VOICE_ASSISTANT_TIMER_FINISHED = 3,
};
#endif
#ifdef USE_ALARM_CONTROL_PANEL
enum AlarmControlPanelState : uint32_t {
  ALARM_STATE_DISARMED = 0,
  ALARM_STATE_ARMED_HOME = 1,
  ALARM_STATE_ARMED_AWAY = 2,
  ALARM_STATE_ARMED_NIGHT = 3,
  ALARM_STATE_ARMED_VACATION = 4,
  ALARM_STATE_ARMED_CUSTOM_BYPASS = 5,
  ALARM_STATE_PENDING = 6,
  ALARM_STATE_ARMING = 7,
  ALARM_STATE_DISARMING = 8,
  ALARM_STATE_TRIGGERED = 9,
};
enum AlarmControlPanelStateCommand : uint32_t {
  ALARM_CONTROL_PANEL_DISARM = 0,
  ALARM_CONTROL_PANEL_ARM_AWAY = 1,
  ALARM_CONTROL_PANEL_ARM_HOME = 2,
  ALARM_CONTROL_PANEL_ARM_NIGHT = 3,
  ALARM_CONTROL_PANEL_ARM_VACATION = 4,
  ALARM_CONTROL_PANEL_ARM_CUSTOM_BYPASS = 5,
  ALARM_CONTROL_PANEL_TRIGGER = 6,
};
#endif
#ifdef USE_TEXT
enum TextMode : uint32_t {
  TEXT_MODE_TEXT = 0,
  TEXT_MODE_PASSWORD = 1,
};
#endif
#ifdef USE_VALVE
enum ValveOperation : uint32_t {
  VALVE_OPERATION_IDLE = 0,
  VALVE_OPERATION_IS_OPENING = 1,
  VALVE_OPERATION_IS_CLOSING = 2,
};
#endif
#ifdef USE_UPDATE
enum UpdateCommand : uint32_t {
  UPDATE_COMMAND_NONE = 0,
  UPDATE_COMMAND_UPDATE = 1,
  UPDATE_COMMAND_CHECK = 2,
};
#endif

}  // namespace enums

class InfoResponseProtoMessage : public ProtoMessage {
 public:
  ~InfoResponseProtoMessage() override = default;
  std::string object_id{};
  uint32_t key{0};
  std::string name{};
  std::string unique_id{};
  bool disabled_by_default{false};
  std::string icon{};
  enums::EntityCategory entity_category{};
  uint32_t device_id{0};

 protected:
};

class StateResponseProtoMessage : public ProtoMessage {
 public:
  ~StateResponseProtoMessage() override = default;
  uint32_t key{0};
  uint32_t device_id{0};

 protected:
};

class CommandProtoMessage : public ProtoMessage {
 public:
  ~CommandProtoMessage() override = default;
  uint32_t key{0};
  uint32_t device_id{0};

 protected:
};
class HelloRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 1;
  static constexpr uint8_t ESTIMATED_SIZE = 17;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "hello_request"; }
#endif
  std::string client_info{};
  uint32_t api_version_major{0};
  uint32_t api_version_minor{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class HelloResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 2;
  static constexpr uint8_t ESTIMATED_SIZE = 26;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "hello_response"; }
#endif
  uint32_t api_version_major{0};
  uint32_t api_version_minor{0};
  std::string server_info{};
  std::string name{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ConnectRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 3;
  static constexpr uint8_t ESTIMATED_SIZE = 9;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "connect_request"; }
#endif
  std::string password{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class ConnectResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 4;
  static constexpr uint8_t ESTIMATED_SIZE = 2;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "connect_response"; }
#endif
  bool invalid_password{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class DisconnectRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 5;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "disconnect_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class DisconnectResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 6;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "disconnect_response"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class PingRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 7;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "ping_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class PingResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 8;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "ping_response"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class DeviceInfoRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 9;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "device_info_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class AreaInfo : public ProtoMessage {
 public:
  uint32_t area_id{0};
  std::string name{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class DeviceInfo : public ProtoMessage {
 public:
  uint32_t device_id{0};
  std::string name{};
  uint32_t area_id{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class DeviceInfoResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 10;
  static constexpr uint8_t ESTIMATED_SIZE = 219;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "device_info_response"; }
#endif
  bool uses_password{false};
  std::string name{};
  std::string mac_address{};
  std::string esphome_version{};
  std::string compilation_time{};
  std::string model{};
  bool has_deep_sleep{false};
  std::string project_name{};
  std::string project_version{};
  uint32_t webserver_port{0};
  uint32_t legacy_bluetooth_proxy_version{0};
  uint32_t bluetooth_proxy_feature_flags{0};
  std::string manufacturer{};
  std::string friendly_name{};
  uint32_t legacy_voice_assistant_version{0};
  uint32_t voice_assistant_feature_flags{0};
  std::string suggested_area{};
  std::string bluetooth_mac_address{};
  bool api_encryption_supported{false};
  std::vector<DeviceInfo> devices{};
  std::vector<AreaInfo> areas{};
  AreaInfo area{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ListEntitiesRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 11;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ListEntitiesDoneResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 19;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_done_response"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SubscribeStatesRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 20;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "subscribe_states_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#ifdef USE_BINARY_SENSOR
class ListEntitiesBinarySensorResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 12;
  static constexpr uint8_t ESTIMATED_SIZE = 60;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_binary_sensor_response"; }
#endif
  std::string device_class{};
  bool is_status_binary_sensor{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BinarySensorStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 21;
  static constexpr uint8_t ESTIMATED_SIZE = 13;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "binary_sensor_state_response"; }
#endif
  bool state{false};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
#ifdef USE_COVER
class ListEntitiesCoverResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 13;
  static constexpr uint8_t ESTIMATED_SIZE = 66;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_cover_response"; }
#endif
  bool assumed_state{false};
  bool supports_position{false};
  bool supports_tilt{false};
  std::string device_class{};
  bool supports_stop{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class CoverStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 22;
  static constexpr uint8_t ESTIMATED_SIZE = 23;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "cover_state_response"; }
#endif
  enums::LegacyCoverState legacy_state{};
  float position{0.0f};
  float tilt{0.0f};
  enums::CoverOperation current_operation{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class CoverCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 30;
  static constexpr uint8_t ESTIMATED_SIZE = 29;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "cover_command_request"; }
#endif
  bool has_legacy_command{false};
  enums::LegacyCoverCommand legacy_command{};
  bool has_position{false};
  float position{0.0f};
  bool has_tilt{false};
  float tilt{0.0f};
  bool stop{false};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_FAN
class ListEntitiesFanResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 14;
  static constexpr uint8_t ESTIMATED_SIZE = 77;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_fan_response"; }
#endif
  bool supports_oscillation{false};
  bool supports_speed{false};
  bool supports_direction{false};
  int32_t supported_speed_count{0};
  std::vector<std::string> supported_preset_modes{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class FanStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 23;
  static constexpr uint8_t ESTIMATED_SIZE = 30;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "fan_state_response"; }
#endif
  bool state{false};
  bool oscillating{false};
  enums::FanSpeed speed{};
  enums::FanDirection direction{};
  int32_t speed_level{0};
  std::string preset_mode{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class FanCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 31;
  static constexpr uint8_t ESTIMATED_SIZE = 42;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "fan_command_request"; }
#endif
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
  bool has_preset_mode{false};
  std::string preset_mode{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_LIGHT
class ListEntitiesLightResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 15;
  static constexpr uint8_t ESTIMATED_SIZE = 90;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_light_response"; }
#endif
  std::vector<enums::ColorMode> supported_color_modes{};
  bool legacy_supports_brightness{false};
  bool legacy_supports_rgb{false};
  bool legacy_supports_white_value{false};
  bool legacy_supports_color_temperature{false};
  float min_mireds{0.0f};
  float max_mireds{0.0f};
  std::vector<std::string> effects{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class LightStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 24;
  static constexpr uint8_t ESTIMATED_SIZE = 67;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "light_state_response"; }
#endif
  bool state{false};
  float brightness{0.0f};
  enums::ColorMode color_mode{};
  float color_brightness{0.0f};
  float red{0.0f};
  float green{0.0f};
  float blue{0.0f};
  float white{0.0f};
  float color_temperature{0.0f};
  float cold_white{0.0f};
  float warm_white{0.0f};
  std::string effect{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class LightCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 32;
  static constexpr uint8_t ESTIMATED_SIZE = 112;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "light_command_request"; }
#endif
  bool has_state{false};
  bool state{false};
  bool has_brightness{false};
  float brightness{0.0f};
  bool has_color_mode{false};
  enums::ColorMode color_mode{};
  bool has_color_brightness{false};
  float color_brightness{0.0f};
  bool has_rgb{false};
  float red{0.0f};
  float green{0.0f};
  float blue{0.0f};
  bool has_white{false};
  float white{0.0f};
  bool has_color_temperature{false};
  float color_temperature{0.0f};
  bool has_cold_white{false};
  float cold_white{0.0f};
  bool has_warm_white{false};
  float warm_white{0.0f};
  bool has_transition_length{false};
  uint32_t transition_length{0};
  bool has_flash_length{false};
  uint32_t flash_length{0};
  bool has_effect{false};
  std::string effect{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_SENSOR
class ListEntitiesSensorResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 16;
  static constexpr uint8_t ESTIMATED_SIZE = 77;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_sensor_response"; }
#endif
  std::string unit_of_measurement{};
  int32_t accuracy_decimals{0};
  bool force_update{false};
  std::string device_class{};
  enums::SensorStateClass state_class{};
  enums::SensorLastResetType legacy_last_reset_type{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SensorStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 25;
  static constexpr uint8_t ESTIMATED_SIZE = 16;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "sensor_state_response"; }
#endif
  float state{0.0f};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
#ifdef USE_SWITCH
class ListEntitiesSwitchResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 17;
  static constexpr uint8_t ESTIMATED_SIZE = 60;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_switch_response"; }
#endif
  bool assumed_state{false};
  std::string device_class{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SwitchStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 26;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "switch_state_response"; }
#endif
  bool state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SwitchCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 33;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "switch_command_request"; }
#endif
  bool state{false};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_TEXT_SENSOR
class ListEntitiesTextSensorResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 18;
  static constexpr uint8_t ESTIMATED_SIZE = 58;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_text_sensor_response"; }
#endif
  std::string device_class{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class TextSensorStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 27;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "text_sensor_state_response"; }
#endif
  std::string state{};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
class SubscribeLogsRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 28;
  static constexpr uint8_t ESTIMATED_SIZE = 4;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "subscribe_logs_request"; }
#endif
  enums::LogLevel level{};
  bool dump_config{false};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class SubscribeLogsResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 29;
  static constexpr uint8_t ESTIMATED_SIZE = 13;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "subscribe_logs_response"; }
#endif
  enums::LogLevel level{};
  std::string message{};
  bool send_failed{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#ifdef USE_API_NOISE
class NoiseEncryptionSetKeyRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 124;
  static constexpr uint8_t ESTIMATED_SIZE = 9;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "noise_encryption_set_key_request"; }
#endif
  std::string key{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class NoiseEncryptionSetKeyResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 125;
  static constexpr uint8_t ESTIMATED_SIZE = 2;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "noise_encryption_set_key_response"; }
#endif
  bool success{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
class SubscribeHomeassistantServicesRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 34;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "subscribe_homeassistant_services_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class HomeassistantServiceMap : public ProtoMessage {
 public:
  std::string key{};
  std::string value{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class HomeassistantServiceResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 35;
  static constexpr uint8_t ESTIMATED_SIZE = 113;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "homeassistant_service_response"; }
#endif
  std::string service{};
  std::vector<HomeassistantServiceMap> data{};
  std::vector<HomeassistantServiceMap> data_template{};
  std::vector<HomeassistantServiceMap> variables{};
  bool is_event{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SubscribeHomeAssistantStatesRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 38;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "subscribe_home_assistant_states_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SubscribeHomeAssistantStateResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 39;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "subscribe_home_assistant_state_response"; }
#endif
  std::string entity_id{};
  std::string attribute{};
  bool once{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class HomeAssistantStateResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 40;
  static constexpr uint8_t ESTIMATED_SIZE = 27;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "home_assistant_state_response"; }
#endif
  std::string entity_id{};
  std::string state{};
  std::string attribute{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class GetTimeRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 36;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "get_time_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class GetTimeResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 37;
  static constexpr uint8_t ESTIMATED_SIZE = 5;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "get_time_response"; }
#endif
  uint32_t epoch_seconds{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
};
#ifdef USE_API_SERVICES
class ListEntitiesServicesArgument : public ProtoMessage {
 public:
  std::string name{};
  enums::ServiceArgType type{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesServicesResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 41;
  static constexpr uint8_t ESTIMATED_SIZE = 48;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_services_response"; }
#endif
  std::string name{};
  uint32_t key{0};
  std::vector<ListEntitiesServicesArgument> args{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
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
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ExecuteServiceRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 42;
  static constexpr uint8_t ESTIMATED_SIZE = 39;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "execute_service_request"; }
#endif
  uint32_t key{0};
  std::vector<ExecuteServiceArgument> args{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
#endif
#ifdef USE_CAMERA
class ListEntitiesCameraResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 43;
  static constexpr uint8_t ESTIMATED_SIZE = 49;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_camera_response"; }
#endif
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class CameraImageResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 44;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "camera_image_response"; }
#endif
  std::string data{};
  bool done{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class CameraImageRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 45;
  static constexpr uint8_t ESTIMATED_SIZE = 4;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "camera_image_request"; }
#endif
  bool single{false};
  bool stream{false};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_CLIMATE
class ListEntitiesClimateResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 46;
  static constexpr uint8_t ESTIMATED_SIZE = 156;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_climate_response"; }
#endif
  bool supports_current_temperature{false};
  bool supports_two_point_target_temperature{false};
  std::vector<enums::ClimateMode> supported_modes{};
  float visual_min_temperature{0.0f};
  float visual_max_temperature{0.0f};
  float visual_target_temperature_step{0.0f};
  bool legacy_supports_away{false};
  bool supports_action{false};
  std::vector<enums::ClimateFanMode> supported_fan_modes{};
  std::vector<enums::ClimateSwingMode> supported_swing_modes{};
  std::vector<std::string> supported_custom_fan_modes{};
  std::vector<enums::ClimatePreset> supported_presets{};
  std::vector<std::string> supported_custom_presets{};
  float visual_current_temperature_step{0.0f};
  bool supports_current_humidity{false};
  bool supports_target_humidity{false};
  float visual_min_humidity{0.0f};
  float visual_max_humidity{0.0f};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ClimateStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 47;
  static constexpr uint8_t ESTIMATED_SIZE = 70;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "climate_state_response"; }
#endif
  enums::ClimateMode mode{};
  float current_temperature{0.0f};
  float target_temperature{0.0f};
  float target_temperature_low{0.0f};
  float target_temperature_high{0.0f};
  bool unused_legacy_away{false};
  enums::ClimateAction action{};
  enums::ClimateFanMode fan_mode{};
  enums::ClimateSwingMode swing_mode{};
  std::string custom_fan_mode{};
  enums::ClimatePreset preset{};
  std::string custom_preset{};
  float current_humidity{0.0f};
  float target_humidity{0.0f};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ClimateCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 48;
  static constexpr uint8_t ESTIMATED_SIZE = 88;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "climate_command_request"; }
#endif
  bool has_mode{false};
  enums::ClimateMode mode{};
  bool has_target_temperature{false};
  float target_temperature{0.0f};
  bool has_target_temperature_low{false};
  float target_temperature_low{0.0f};
  bool has_target_temperature_high{false};
  float target_temperature_high{0.0f};
  bool unused_has_legacy_away{false};
  bool unused_legacy_away{false};
  bool has_fan_mode{false};
  enums::ClimateFanMode fan_mode{};
  bool has_swing_mode{false};
  enums::ClimateSwingMode swing_mode{};
  bool has_custom_fan_mode{false};
  std::string custom_fan_mode{};
  bool has_preset{false};
  enums::ClimatePreset preset{};
  bool has_custom_preset{false};
  std::string custom_preset{};
  bool has_target_humidity{false};
  float target_humidity{0.0f};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_NUMBER
class ListEntitiesNumberResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 49;
  static constexpr uint8_t ESTIMATED_SIZE = 84;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_number_response"; }
#endif
  float min_value{0.0f};
  float max_value{0.0f};
  float step{0.0f};
  std::string unit_of_measurement{};
  enums::NumberMode mode{};
  std::string device_class{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class NumberStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 50;
  static constexpr uint8_t ESTIMATED_SIZE = 16;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "number_state_response"; }
#endif
  float state{0.0f};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class NumberCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 51;
  static constexpr uint8_t ESTIMATED_SIZE = 14;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "number_command_request"; }
#endif
  float state{0.0f};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_SELECT
class ListEntitiesSelectResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 52;
  static constexpr uint8_t ESTIMATED_SIZE = 67;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_select_response"; }
#endif
  std::vector<std::string> options{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SelectStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 53;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "select_state_response"; }
#endif
  std::string state{};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SelectCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 54;
  static constexpr uint8_t ESTIMATED_SIZE = 18;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "select_command_request"; }
#endif
  std::string state{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_SIREN
class ListEntitiesSirenResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 55;
  static constexpr uint8_t ESTIMATED_SIZE = 71;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_siren_response"; }
#endif
  std::vector<std::string> tones{};
  bool supports_duration{false};
  bool supports_volume{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SirenStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 56;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "siren_state_response"; }
#endif
  bool state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SirenCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 57;
  static constexpr uint8_t ESTIMATED_SIZE = 37;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "siren_command_request"; }
#endif
  bool has_state{false};
  bool state{false};
  bool has_tone{false};
  std::string tone{};
  bool has_duration{false};
  uint32_t duration{0};
  bool has_volume{false};
  float volume{0.0f};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_LOCK
class ListEntitiesLockResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 58;
  static constexpr uint8_t ESTIMATED_SIZE = 64;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_lock_response"; }
#endif
  bool assumed_state{false};
  bool supports_open{false};
  bool requires_code{false};
  std::string code_format{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class LockStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 59;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "lock_state_response"; }
#endif
  enums::LockState state{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class LockCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 60;
  static constexpr uint8_t ESTIMATED_SIZE = 22;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "lock_command_request"; }
#endif
  enums::LockCommand command{};
  bool has_code{false};
  std::string code{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_BUTTON
class ListEntitiesButtonResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 61;
  static constexpr uint8_t ESTIMATED_SIZE = 58;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_button_response"; }
#endif
  std::string device_class{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ButtonCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 62;
  static constexpr uint8_t ESTIMATED_SIZE = 9;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "button_command_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_MEDIA_PLAYER
class MediaPlayerSupportedFormat : public ProtoMessage {
 public:
  std::string format{};
  uint32_t sample_rate{0};
  uint32_t num_channels{0};
  enums::MediaPlayerFormatPurpose purpose{};
  uint32_t sample_bytes{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ListEntitiesMediaPlayerResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 63;
  static constexpr uint8_t ESTIMATED_SIZE = 85;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_media_player_response"; }
#endif
  bool supports_pause{false};
  std::vector<MediaPlayerSupportedFormat> supported_formats{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class MediaPlayerStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 64;
  static constexpr uint8_t ESTIMATED_SIZE = 18;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "media_player_state_response"; }
#endif
  enums::MediaPlayerState state{};
  float volume{0.0f};
  bool muted{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class MediaPlayerCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 65;
  static constexpr uint8_t ESTIMATED_SIZE = 35;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "media_player_command_request"; }
#endif
  bool has_command{false};
  enums::MediaPlayerCommand command{};
  bool has_volume{false};
  float volume{0.0f};
  bool has_media_url{false};
  std::string media_url{};
  bool has_announcement{false};
  bool announcement{false};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_BLUETOOTH_PROXY
class SubscribeBluetoothLEAdvertisementsRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 66;
  static constexpr uint8_t ESTIMATED_SIZE = 4;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "subscribe_bluetooth_le_advertisements_request"; }
#endif
  uint32_t flags{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothServiceData : public ProtoMessage {
 public:
  std::string uuid{};
  std::vector<uint32_t> legacy_data{};
  std::string data{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothLEAdvertisementResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 67;
  static constexpr uint8_t ESTIMATED_SIZE = 107;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_le_advertisement_response"; }
#endif
  uint64_t address{0};
  std::string name{};
  int32_t rssi{0};
  std::vector<std::string> service_uuids{};
  std::vector<BluetoothServiceData> service_data{};
  std::vector<BluetoothServiceData> manufacturer_data{};
  uint32_t address_type{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothLERawAdvertisement : public ProtoMessage {
 public:
  uint64_t address{0};
  int32_t rssi{0};
  uint32_t address_type{0};
  std::string data{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothLERawAdvertisementsResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 93;
  static constexpr uint8_t ESTIMATED_SIZE = 34;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_le_raw_advertisements_response"; }
#endif
  std::vector<BluetoothLERawAdvertisement> advertisements{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothDeviceRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 68;
  static constexpr uint8_t ESTIMATED_SIZE = 12;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_device_request"; }
#endif
  uint64_t address{0};
  enums::BluetoothDeviceRequestType request_type{};
  bool has_address_type{false};
  uint32_t address_type{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothDeviceConnectionResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 69;
  static constexpr uint8_t ESTIMATED_SIZE = 14;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_device_connection_response"; }
#endif
  uint64_t address{0};
  bool connected{false};
  uint32_t mtu{0};
  int32_t error{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTGetServicesRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 70;
  static constexpr uint8_t ESTIMATED_SIZE = 4;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_get_services_request"; }
#endif
  uint64_t address{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothGATTDescriptor : public ProtoMessage {
 public:
  std::vector<uint64_t> uuid{};
  uint32_t handle{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothGATTCharacteristic : public ProtoMessage {
 public:
  std::vector<uint64_t> uuid{};
  uint32_t handle{0};
  uint32_t properties{0};
  std::vector<BluetoothGATTDescriptor> descriptors{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothGATTService : public ProtoMessage {
 public:
  std::vector<uint64_t> uuid{};
  uint32_t handle{0};
  std::vector<BluetoothGATTCharacteristic> characteristics{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothGATTGetServicesResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 71;
  static constexpr uint8_t ESTIMATED_SIZE = 38;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_get_services_response"; }
#endif
  uint64_t address{0};
  std::vector<BluetoothGATTService> services{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTGetServicesDoneResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 72;
  static constexpr uint8_t ESTIMATED_SIZE = 4;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_get_services_done_response"; }
#endif
  uint64_t address{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTReadRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 73;
  static constexpr uint8_t ESTIMATED_SIZE = 8;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_read_request"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothGATTReadResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 74;
  static constexpr uint8_t ESTIMATED_SIZE = 17;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_read_response"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  std::string data{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTWriteRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 75;
  static constexpr uint8_t ESTIMATED_SIZE = 19;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_write_request"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  bool response{false};
  std::string data{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothGATTReadDescriptorRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 76;
  static constexpr uint8_t ESTIMATED_SIZE = 8;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_read_descriptor_request"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothGATTWriteDescriptorRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 77;
  static constexpr uint8_t ESTIMATED_SIZE = 17;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_write_descriptor_request"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  std::string data{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothGATTNotifyRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 78;
  static constexpr uint8_t ESTIMATED_SIZE = 10;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_notify_request"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  bool enable{false};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothGATTNotifyDataResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 79;
  static constexpr uint8_t ESTIMATED_SIZE = 17;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_notify_data_response"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  std::string data{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SubscribeBluetoothConnectionsFreeRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 80;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "subscribe_bluetooth_connections_free_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothConnectionsFreeResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 81;
  static constexpr uint8_t ESTIMATED_SIZE = 16;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_connections_free_response"; }
#endif
  uint32_t free{0};
  uint32_t limit{0};
  std::vector<uint64_t> allocated{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTErrorResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 82;
  static constexpr uint8_t ESTIMATED_SIZE = 12;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_error_response"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  int32_t error{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTWriteResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 83;
  static constexpr uint8_t ESTIMATED_SIZE = 8;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_write_response"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTNotifyResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 84;
  static constexpr uint8_t ESTIMATED_SIZE = 8;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_notify_response"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothDevicePairingResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 85;
  static constexpr uint8_t ESTIMATED_SIZE = 10;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_device_pairing_response"; }
#endif
  uint64_t address{0};
  bool paired{false};
  int32_t error{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothDeviceUnpairingResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 86;
  static constexpr uint8_t ESTIMATED_SIZE = 10;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_device_unpairing_response"; }
#endif
  uint64_t address{0};
  bool success{false};
  int32_t error{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class UnsubscribeBluetoothLEAdvertisementsRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 87;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "unsubscribe_bluetooth_le_advertisements_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothDeviceClearCacheResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 88;
  static constexpr uint8_t ESTIMATED_SIZE = 10;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_device_clear_cache_response"; }
#endif
  uint64_t address{0};
  bool success{false};
  int32_t error{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothScannerStateResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 126;
  static constexpr uint8_t ESTIMATED_SIZE = 4;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_scanner_state_response"; }
#endif
  enums::BluetoothScannerState state{};
  enums::BluetoothScannerMode mode{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothScannerSetModeRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 127;
  static constexpr uint8_t ESTIMATED_SIZE = 2;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_scanner_set_mode_request"; }
#endif
  enums::BluetoothScannerMode mode{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_VOICE_ASSISTANT
class SubscribeVoiceAssistantRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 89;
  static constexpr uint8_t ESTIMATED_SIZE = 6;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "subscribe_voice_assistant_request"; }
#endif
  bool subscribe{false};
  uint32_t flags{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class VoiceAssistantAudioSettings : public ProtoMessage {
 public:
  uint32_t noise_suppression_level{0};
  uint32_t auto_gain{0};
  float volume_multiplier{0.0f};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class VoiceAssistantRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 90;
  static constexpr uint8_t ESTIMATED_SIZE = 41;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_request"; }
#endif
  bool start{false};
  std::string conversation_id{};
  uint32_t flags{0};
  VoiceAssistantAudioSettings audio_settings{};
  std::string wake_word_phrase{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class VoiceAssistantResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 91;
  static constexpr uint8_t ESTIMATED_SIZE = 6;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_response"; }
#endif
  uint32_t port{0};
  bool error{false};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class VoiceAssistantEventData : public ProtoMessage {
 public:
  std::string name{};
  std::string value{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class VoiceAssistantEventResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 92;
  static constexpr uint8_t ESTIMATED_SIZE = 36;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_event_response"; }
#endif
  enums::VoiceAssistantEvent event_type{};
  std::vector<VoiceAssistantEventData> data{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class VoiceAssistantAudio : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 106;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_audio"; }
#endif
  std::string data{};
  bool end{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class VoiceAssistantTimerEventResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 115;
  static constexpr uint8_t ESTIMATED_SIZE = 30;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_timer_event_response"; }
#endif
  enums::VoiceAssistantTimerEvent event_type{};
  std::string timer_id{};
  std::string name{};
  uint32_t total_seconds{0};
  uint32_t seconds_left{0};
  bool is_active{false};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class VoiceAssistantAnnounceRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 119;
  static constexpr uint8_t ESTIMATED_SIZE = 29;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_announce_request"; }
#endif
  std::string media_id{};
  std::string text{};
  std::string preannounce_media_id{};
  bool start_conversation{false};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class VoiceAssistantAnnounceFinished : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 120;
  static constexpr uint8_t ESTIMATED_SIZE = 2;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_announce_finished"; }
#endif
  bool success{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class VoiceAssistantWakeWord : public ProtoMessage {
 public:
  std::string id{};
  std::string wake_word{};
  std::vector<std::string> trained_languages{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class VoiceAssistantConfigurationRequest : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 121;
  static constexpr uint8_t ESTIMATED_SIZE = 0;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_configuration_request"; }
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class VoiceAssistantConfigurationResponse : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 122;
  static constexpr uint8_t ESTIMATED_SIZE = 56;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_configuration_response"; }
#endif
  std::vector<VoiceAssistantWakeWord> available_wake_words{};
  std::vector<std::string> active_wake_words{};
  uint32_t max_active_wake_words{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class VoiceAssistantSetConfiguration : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 123;
  static constexpr uint8_t ESTIMATED_SIZE = 18;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_set_configuration"; }
#endif
  std::vector<std::string> active_wake_words{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
#endif
#ifdef USE_ALARM_CONTROL_PANEL
class ListEntitiesAlarmControlPanelResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 94;
  static constexpr uint8_t ESTIMATED_SIZE = 57;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_alarm_control_panel_response"; }
#endif
  uint32_t supported_features{0};
  bool requires_code{false};
  bool requires_code_to_arm{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class AlarmControlPanelStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 95;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "alarm_control_panel_state_response"; }
#endif
  enums::AlarmControlPanelState state{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class AlarmControlPanelCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 96;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "alarm_control_panel_command_request"; }
#endif
  enums::AlarmControlPanelStateCommand command{};
  std::string code{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_TEXT
class ListEntitiesTextResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 97;
  static constexpr uint8_t ESTIMATED_SIZE = 68;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_text_response"; }
#endif
  uint32_t min_length{0};
  uint32_t max_length{0};
  std::string pattern{};
  enums::TextMode mode{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class TextStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 98;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "text_state_response"; }
#endif
  std::string state{};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class TextCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 99;
  static constexpr uint8_t ESTIMATED_SIZE = 18;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "text_command_request"; }
#endif
  std::string state{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_DATETIME_DATE
class ListEntitiesDateResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 100;
  static constexpr uint8_t ESTIMATED_SIZE = 49;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_date_response"; }
#endif
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class DateStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 101;
  static constexpr uint8_t ESTIMATED_SIZE = 23;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "date_state_response"; }
#endif
  bool missing_state{false};
  uint32_t year{0};
  uint32_t month{0};
  uint32_t day{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class DateCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 102;
  static constexpr uint8_t ESTIMATED_SIZE = 21;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "date_command_request"; }
#endif
  uint32_t year{0};
  uint32_t month{0};
  uint32_t day{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_DATETIME_TIME
class ListEntitiesTimeResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 103;
  static constexpr uint8_t ESTIMATED_SIZE = 49;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_time_response"; }
#endif
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class TimeStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 104;
  static constexpr uint8_t ESTIMATED_SIZE = 23;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "time_state_response"; }
#endif
  bool missing_state{false};
  uint32_t hour{0};
  uint32_t minute{0};
  uint32_t second{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class TimeCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 105;
  static constexpr uint8_t ESTIMATED_SIZE = 21;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "time_command_request"; }
#endif
  uint32_t hour{0};
  uint32_t minute{0};
  uint32_t second{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_EVENT
class ListEntitiesEventResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 107;
  static constexpr uint8_t ESTIMATED_SIZE = 76;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_event_response"; }
#endif
  std::string device_class{};
  std::vector<std::string> event_types{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class EventResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 108;
  static constexpr uint8_t ESTIMATED_SIZE = 18;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "event_response"; }
#endif
  std::string event_type{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
#ifdef USE_VALVE
class ListEntitiesValveResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 109;
  static constexpr uint8_t ESTIMATED_SIZE = 64;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_valve_response"; }
#endif
  std::string device_class{};
  bool assumed_state{false};
  bool supports_position{false};
  bool supports_stop{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ValveStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 110;
  static constexpr uint8_t ESTIMATED_SIZE = 16;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "valve_state_response"; }
#endif
  float position{0.0f};
  enums::ValveOperation current_operation{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ValveCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 111;
  static constexpr uint8_t ESTIMATED_SIZE = 18;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "valve_command_request"; }
#endif
  bool has_position{false};
  float position{0.0f};
  bool stop{false};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_DATETIME_DATETIME
class ListEntitiesDateTimeResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 112;
  static constexpr uint8_t ESTIMATED_SIZE = 49;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_date_time_response"; }
#endif
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class DateTimeStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 113;
  static constexpr uint8_t ESTIMATED_SIZE = 16;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "date_time_state_response"; }
#endif
  bool missing_state{false};
  uint32_t epoch_seconds{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class DateTimeCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 114;
  static constexpr uint8_t ESTIMATED_SIZE = 14;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "date_time_command_request"; }
#endif
  uint32_t epoch_seconds{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_UPDATE
class ListEntitiesUpdateResponse : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 116;
  static constexpr uint8_t ESTIMATED_SIZE = 58;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_update_response"; }
#endif
  std::string device_class{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class UpdateStateResponse : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 117;
  static constexpr uint8_t ESTIMATED_SIZE = 65;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "update_state_response"; }
#endif
  bool missing_state{false};
  bool in_progress{false};
  bool has_progress{false};
  float progress{0.0f};
  std::string current_version{};
  std::string latest_version{};
  std::string title{};
  std::string release_summary{};
  std::string release_url{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(uint32_t &total_size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class UpdateCommandRequest : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 118;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "update_command_request"; }
#endif
  enums::UpdateCommand command{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif

}  // namespace api
}  // namespace esphome
