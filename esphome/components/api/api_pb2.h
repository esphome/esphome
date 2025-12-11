// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/string_ref.h"

#include "proto.h"
#include "api_pb2_includes.h"

namespace esphome::api {

namespace enums {

enum EntityCategory : uint32_t {
  ENTITY_CATEGORY_NONE = 0,
  ENTITY_CATEGORY_CONFIG = 1,
  ENTITY_CATEGORY_DIAGNOSTIC = 2,
};
#ifdef USE_COVER
enum CoverOperation : uint32_t {
  COVER_OPERATION_IDLE = 0,
  COVER_OPERATION_IS_OPENING = 1,
  COVER_OPERATION_IS_CLOSING = 2,
};
#endif
#ifdef USE_FAN
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
  STATE_CLASS_MEASUREMENT_ANGLE = 4,
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
#ifdef USE_API_USER_DEFINED_ACTIONS
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
enum SupportsResponseType : uint32_t {
  SUPPORTS_RESPONSE_NONE = 0,
  SUPPORTS_RESPONSE_OPTIONAL = 1,
  SUPPORTS_RESPONSE_ONLY = 2,
  SUPPORTS_RESPONSE_STATUS = 100,
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
  MEDIA_PLAYER_STATE_ANNOUNCING = 4,
  MEDIA_PLAYER_STATE_OFF = 5,
  MEDIA_PLAYER_STATE_ON = 6,
};
enum MediaPlayerCommand : uint32_t {
  MEDIA_PLAYER_COMMAND_PLAY = 0,
  MEDIA_PLAYER_COMMAND_PAUSE = 1,
  MEDIA_PLAYER_COMMAND_STOP = 2,
  MEDIA_PLAYER_COMMAND_MUTE = 3,
  MEDIA_PLAYER_COMMAND_UNMUTE = 4,
  MEDIA_PLAYER_COMMAND_TOGGLE = 5,
  MEDIA_PLAYER_COMMAND_VOLUME_UP = 6,
  MEDIA_PLAYER_COMMAND_VOLUME_DOWN = 7,
  MEDIA_PLAYER_COMMAND_ENQUEUE = 8,
  MEDIA_PLAYER_COMMAND_REPEAT_ONE = 9,
  MEDIA_PLAYER_COMMAND_REPEAT_OFF = 10,
  MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST = 11,
  MEDIA_PLAYER_COMMAND_TURN_ON = 12,
  MEDIA_PLAYER_COMMAND_TURN_OFF = 13,
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
#ifdef USE_ZWAVE_PROXY
enum ZWaveProxyRequestType : uint32_t {
  ZWAVE_PROXY_REQUEST_TYPE_SUBSCRIBE = 0,
  ZWAVE_PROXY_REQUEST_TYPE_UNSUBSCRIBE = 1,
  ZWAVE_PROXY_REQUEST_TYPE_HOME_ID_CHANGE = 2,
};
#endif

}  // namespace enums

class InfoResponseProtoMessage : public ProtoMessage {
 public:
  ~InfoResponseProtoMessage() override = default;
  StringRef object_id_ref_{};
  void set_object_id(const StringRef &ref) { this->object_id_ref_ = ref; }
  uint32_t key{0};
  StringRef name_ref_{};
  void set_name(const StringRef &ref) { this->name_ref_ = ref; }
  bool disabled_by_default{false};
#ifdef USE_ENTITY_ICON
  StringRef icon_ref_{};
  void set_icon(const StringRef &ref) { this->icon_ref_ = ref; }
#endif
  enums::EntityCategory entity_category{};
#ifdef USE_DEVICES
  uint32_t device_id{0};
#endif

 protected:
};

class StateResponseProtoMessage : public ProtoMessage {
 public:
  ~StateResponseProtoMessage() override = default;
  uint32_t key{0};
#ifdef USE_DEVICES
  uint32_t device_id{0};
#endif

 protected:
};

class CommandProtoMessage : public ProtoDecodableMessage {
 public:
  ~CommandProtoMessage() override = default;
  uint32_t key{0};
#ifdef USE_DEVICES
  uint32_t device_id{0};
#endif

 protected:
};
class HelloRequest final : public ProtoDecodableMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 1;
  static constexpr uint8_t ESTIMATED_SIZE = 27;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "hello_request"; }
#endif
  const uint8_t *client_info{nullptr};
  uint16_t client_info_len{0};
  uint32_t api_version_major{0};
  uint32_t api_version_minor{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class HelloResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 2;
  static constexpr uint8_t ESTIMATED_SIZE = 26;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "hello_response"; }
#endif
  uint32_t api_version_major{0};
  uint32_t api_version_minor{0};
  StringRef server_info_ref_{};
  void set_server_info(const StringRef &ref) { this->server_info_ref_ = ref; }
  StringRef name_ref_{};
  void set_name(const StringRef &ref) { this->name_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#ifdef USE_API_PASSWORD
class AuthenticationRequest final : public ProtoDecodableMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 3;
  static constexpr uint8_t ESTIMATED_SIZE = 19;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "authentication_request"; }
#endif
  const uint8_t *password{nullptr};
  uint16_t password_len{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class AuthenticationResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 4;
  static constexpr uint8_t ESTIMATED_SIZE = 2;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "authentication_response"; }
#endif
  bool invalid_password{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
class DisconnectRequest final : public ProtoMessage {
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
class DisconnectResponse final : public ProtoMessage {
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
class PingRequest final : public ProtoMessage {
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
class PingResponse final : public ProtoMessage {
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
class DeviceInfoRequest final : public ProtoMessage {
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
#ifdef USE_AREAS
class AreaInfo final : public ProtoMessage {
 public:
  uint32_t area_id{0};
  StringRef name_ref_{};
  void set_name(const StringRef &ref) { this->name_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
#ifdef USE_DEVICES
class DeviceInfo final : public ProtoMessage {
 public:
  uint32_t device_id{0};
  StringRef name_ref_{};
  void set_name(const StringRef &ref) { this->name_ref_ = ref; }
  uint32_t area_id{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
class DeviceInfoResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 10;
  static constexpr uint16_t ESTIMATED_SIZE = 257;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "device_info_response"; }
#endif
#ifdef USE_API_PASSWORD
  bool uses_password{false};
#endif
  StringRef name_ref_{};
  void set_name(const StringRef &ref) { this->name_ref_ = ref; }
  StringRef mac_address_ref_{};
  void set_mac_address(const StringRef &ref) { this->mac_address_ref_ = ref; }
  StringRef esphome_version_ref_{};
  void set_esphome_version(const StringRef &ref) { this->esphome_version_ref_ = ref; }
  StringRef compilation_time_ref_{};
  void set_compilation_time(const StringRef &ref) { this->compilation_time_ref_ = ref; }
  StringRef model_ref_{};
  void set_model(const StringRef &ref) { this->model_ref_ = ref; }
#ifdef USE_DEEP_SLEEP
  bool has_deep_sleep{false};
#endif
#ifdef ESPHOME_PROJECT_NAME
  StringRef project_name_ref_{};
  void set_project_name(const StringRef &ref) { this->project_name_ref_ = ref; }
#endif
#ifdef ESPHOME_PROJECT_NAME
  StringRef project_version_ref_{};
  void set_project_version(const StringRef &ref) { this->project_version_ref_ = ref; }
#endif
#ifdef USE_WEBSERVER
  uint32_t webserver_port{0};
#endif
#ifdef USE_BLUETOOTH_PROXY
  uint32_t bluetooth_proxy_feature_flags{0};
#endif
  StringRef manufacturer_ref_{};
  void set_manufacturer(const StringRef &ref) { this->manufacturer_ref_ = ref; }
  StringRef friendly_name_ref_{};
  void set_friendly_name(const StringRef &ref) { this->friendly_name_ref_ = ref; }
#ifdef USE_VOICE_ASSISTANT
  uint32_t voice_assistant_feature_flags{0};
#endif
#ifdef USE_AREAS
  StringRef suggested_area_ref_{};
  void set_suggested_area(const StringRef &ref) { this->suggested_area_ref_ = ref; }
#endif
#ifdef USE_BLUETOOTH_PROXY
  StringRef bluetooth_mac_address_ref_{};
  void set_bluetooth_mac_address(const StringRef &ref) { this->bluetooth_mac_address_ref_ = ref; }
#endif
#ifdef USE_API_NOISE
  bool api_encryption_supported{false};
#endif
#ifdef USE_DEVICES
  std::array<DeviceInfo, ESPHOME_DEVICE_COUNT> devices{};
#endif
#ifdef USE_AREAS
  std::array<AreaInfo, ESPHOME_AREA_COUNT> areas{};
#endif
#ifdef USE_AREAS
  AreaInfo area{};
#endif
#ifdef USE_ZWAVE_PROXY
  uint32_t zwave_proxy_feature_flags{0};
#endif
#ifdef USE_ZWAVE_PROXY
  uint32_t zwave_home_id{0};
#endif
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ListEntitiesRequest final : public ProtoMessage {
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
class ListEntitiesDoneResponse final : public ProtoMessage {
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
class SubscribeStatesRequest final : public ProtoMessage {
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
class ListEntitiesBinarySensorResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 12;
  static constexpr uint8_t ESTIMATED_SIZE = 51;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_binary_sensor_response"; }
#endif
  StringRef device_class_ref_{};
  void set_device_class(const StringRef &ref) { this->device_class_ref_ = ref; }
  bool is_status_binary_sensor{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BinarySensorStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 21;
  static constexpr uint8_t ESTIMATED_SIZE = 13;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "binary_sensor_state_response"; }
#endif
  bool state{false};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
#ifdef USE_COVER
class ListEntitiesCoverResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 13;
  static constexpr uint8_t ESTIMATED_SIZE = 57;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_cover_response"; }
#endif
  bool assumed_state{false};
  bool supports_position{false};
  bool supports_tilt{false};
  StringRef device_class_ref_{};
  void set_device_class(const StringRef &ref) { this->device_class_ref_ = ref; }
  bool supports_stop{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class CoverStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 22;
  static constexpr uint8_t ESTIMATED_SIZE = 21;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "cover_state_response"; }
#endif
  float position{0.0f};
  float tilt{0.0f};
  enums::CoverOperation current_operation{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class CoverCommandRequest final : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 30;
  static constexpr uint8_t ESTIMATED_SIZE = 25;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "cover_command_request"; }
#endif
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
class ListEntitiesFanResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 14;
  static constexpr uint8_t ESTIMATED_SIZE = 68;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_fan_response"; }
#endif
  bool supports_oscillation{false};
  bool supports_speed{false};
  bool supports_direction{false};
  int32_t supported_speed_count{0};
  const std::vector<const char *> *supported_preset_modes{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class FanStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 23;
  static constexpr uint8_t ESTIMATED_SIZE = 28;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "fan_state_response"; }
#endif
  bool state{false};
  bool oscillating{false};
  enums::FanDirection direction{};
  int32_t speed_level{0};
  StringRef preset_mode_ref_{};
  void set_preset_mode(const StringRef &ref) { this->preset_mode_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class FanCommandRequest final : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 31;
  static constexpr uint8_t ESTIMATED_SIZE = 38;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "fan_command_request"; }
#endif
  bool has_state{false};
  bool state{false};
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
class ListEntitiesLightResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 15;
  static constexpr uint8_t ESTIMATED_SIZE = 73;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_light_response"; }
#endif
  const light::ColorModeMask *supported_color_modes{};
  float min_mireds{0.0f};
  float max_mireds{0.0f};
  const FixedVector<const char *> *effects{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class LightStateResponse final : public StateResponseProtoMessage {
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
  StringRef effect_ref_{};
  void set_effect(const StringRef &ref) { this->effect_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class LightCommandRequest final : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 32;
  static constexpr uint8_t ESTIMATED_SIZE = 122;
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
  const uint8_t *effect{nullptr};
  uint16_t effect_len{0};
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
class ListEntitiesSensorResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 16;
  static constexpr uint8_t ESTIMATED_SIZE = 66;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_sensor_response"; }
#endif
  StringRef unit_of_measurement_ref_{};
  void set_unit_of_measurement(const StringRef &ref) { this->unit_of_measurement_ref_ = ref; }
  int32_t accuracy_decimals{0};
  bool force_update{false};
  StringRef device_class_ref_{};
  void set_device_class(const StringRef &ref) { this->device_class_ref_ = ref; }
  enums::SensorStateClass state_class{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SensorStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 25;
  static constexpr uint8_t ESTIMATED_SIZE = 16;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "sensor_state_response"; }
#endif
  float state{0.0f};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
#ifdef USE_SWITCH
class ListEntitiesSwitchResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 17;
  static constexpr uint8_t ESTIMATED_SIZE = 51;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_switch_response"; }
#endif
  bool assumed_state{false};
  StringRef device_class_ref_{};
  void set_device_class(const StringRef &ref) { this->device_class_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SwitchStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 26;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "switch_state_response"; }
#endif
  bool state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SwitchCommandRequest final : public CommandProtoMessage {
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
class ListEntitiesTextSensorResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 18;
  static constexpr uint8_t ESTIMATED_SIZE = 49;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_text_sensor_response"; }
#endif
  StringRef device_class_ref_{};
  void set_device_class(const StringRef &ref) { this->device_class_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class TextSensorStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 27;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "text_sensor_state_response"; }
#endif
  StringRef state_ref_{};
  void set_state(const StringRef &ref) { this->state_ref_ = ref; }
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
class SubscribeLogsRequest final : public ProtoDecodableMessage {
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
class SubscribeLogsResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 29;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "subscribe_logs_response"; }
#endif
  enums::LogLevel level{};
  const uint8_t *message_ptr_{nullptr};
  size_t message_len_{0};
  void set_message(const uint8_t *data, size_t len) {
    this->message_ptr_ = data;
    this->message_len_ = len;
  }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#ifdef USE_API_NOISE
class NoiseEncryptionSetKeyRequest final : public ProtoDecodableMessage {
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
class NoiseEncryptionSetKeyResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 125;
  static constexpr uint8_t ESTIMATED_SIZE = 2;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "noise_encryption_set_key_response"; }
#endif
  bool success{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
#ifdef USE_API_HOMEASSISTANT_SERVICES
class SubscribeHomeassistantServicesRequest final : public ProtoMessage {
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
class HomeassistantServiceMap final : public ProtoMessage {
 public:
  StringRef key_ref_{};
  void set_key(const StringRef &ref) { this->key_ref_ = ref; }
  std::string value{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class HomeassistantActionRequest final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 35;
  static constexpr uint8_t ESTIMATED_SIZE = 128;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "homeassistant_action_request"; }
#endif
  StringRef service_ref_{};
  void set_service(const StringRef &ref) { this->service_ref_ = ref; }
  FixedVector<HomeassistantServiceMap> data{};
  FixedVector<HomeassistantServiceMap> data_template{};
  FixedVector<HomeassistantServiceMap> variables{};
  bool is_event{false};
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  uint32_t call_id{0};
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  bool wants_response{false};
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  std::string response_template{};
#endif
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
class HomeassistantActionResponse final : public ProtoDecodableMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 130;
  static constexpr uint8_t ESTIMATED_SIZE = 34;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "homeassistant_action_response"; }
#endif
  uint32_t call_id{0};
  bool success{false};
  std::string error_message{};
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  const uint8_t *response_data{nullptr};
  uint16_t response_data_len{0};
#endif
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
class SubscribeHomeAssistantStatesRequest final : public ProtoMessage {
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
class SubscribeHomeAssistantStateResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 39;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "subscribe_home_assistant_state_response"; }
#endif
  StringRef entity_id_ref_{};
  void set_entity_id(const StringRef &ref) { this->entity_id_ref_ = ref; }
  StringRef attribute_ref_{};
  void set_attribute(const StringRef &ref) { this->attribute_ref_ = ref; }
  bool once{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class HomeAssistantStateResponse final : public ProtoDecodableMessage {
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
#endif
class GetTimeRequest final : public ProtoMessage {
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
class GetTimeResponse final : public ProtoDecodableMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 37;
  static constexpr uint8_t ESTIMATED_SIZE = 24;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "get_time_response"; }
#endif
  uint32_t epoch_seconds{0};
  const uint8_t *timezone{nullptr};
  uint16_t timezone_len{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
#ifdef USE_API_USER_DEFINED_ACTIONS
class ListEntitiesServicesArgument final : public ProtoMessage {
 public:
  StringRef name_ref_{};
  void set_name(const StringRef &ref) { this->name_ref_ = ref; }
  enums::ServiceArgType type{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ListEntitiesServicesResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 41;
  static constexpr uint8_t ESTIMATED_SIZE = 50;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_services_response"; }
#endif
  StringRef name_ref_{};
  void set_name(const StringRef &ref) { this->name_ref_ = ref; }
  uint32_t key{0};
  FixedVector<ListEntitiesServicesArgument> args{};
  enums::SupportsResponseType supports_response{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ExecuteServiceArgument final : public ProtoDecodableMessage {
 public:
  bool bool_{false};
  int32_t legacy_int{0};
  float float_{0.0f};
  std::string string_{};
  int32_t int_{0};
  FixedVector<bool> bool_array{};
  FixedVector<int32_t> int_array{};
  FixedVector<float> float_array{};
  FixedVector<std::string> string_array{};
  void decode(const uint8_t *buffer, size_t length) override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class ExecuteServiceRequest final : public ProtoDecodableMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 42;
  static constexpr uint8_t ESTIMATED_SIZE = 45;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "execute_service_request"; }
#endif
  uint32_t key{0};
  FixedVector<ExecuteServiceArgument> args{};
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  uint32_t call_id{0};
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  bool return_response{false};
#endif
  void decode(const uint8_t *buffer, size_t length) override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_32bit(uint32_t field_id, Proto32Bit value) override;
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
class ExecuteServiceResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 131;
  static constexpr uint8_t ESTIMATED_SIZE = 34;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "execute_service_response"; }
#endif
  uint32_t call_id{0};
  bool success{false};
  StringRef error_message_ref_{};
  void set_error_message(const StringRef &ref) { this->error_message_ref_ = ref; }
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  const uint8_t *response_data{nullptr};
  uint16_t response_data_len{0};
#endif
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
#ifdef USE_CAMERA
class ListEntitiesCameraResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 43;
  static constexpr uint8_t ESTIMATED_SIZE = 40;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_camera_response"; }
#endif
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class CameraImageResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 44;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "camera_image_response"; }
#endif
  const uint8_t *data_ptr_{nullptr};
  size_t data_len_{0};
  void set_data(const uint8_t *data, size_t len) {
    this->data_ptr_ = data;
    this->data_len_ = len;
  }
  bool done{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class CameraImageRequest final : public ProtoDecodableMessage {
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
class ListEntitiesClimateResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 46;
  static constexpr uint8_t ESTIMATED_SIZE = 150;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_climate_response"; }
#endif
  bool supports_current_temperature{false};
  bool supports_two_point_target_temperature{false};
  const climate::ClimateModeMask *supported_modes{};
  float visual_min_temperature{0.0f};
  float visual_max_temperature{0.0f};
  float visual_target_temperature_step{0.0f};
  bool supports_action{false};
  const climate::ClimateFanModeMask *supported_fan_modes{};
  const climate::ClimateSwingModeMask *supported_swing_modes{};
  const std::vector<const char *> *supported_custom_fan_modes{};
  const climate::ClimatePresetMask *supported_presets{};
  const std::vector<const char *> *supported_custom_presets{};
  float visual_current_temperature_step{0.0f};
  bool supports_current_humidity{false};
  bool supports_target_humidity{false};
  float visual_min_humidity{0.0f};
  float visual_max_humidity{0.0f};
  uint32_t feature_flags{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ClimateStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 47;
  static constexpr uint8_t ESTIMATED_SIZE = 68;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "climate_state_response"; }
#endif
  enums::ClimateMode mode{};
  float current_temperature{0.0f};
  float target_temperature{0.0f};
  float target_temperature_low{0.0f};
  float target_temperature_high{0.0f};
  enums::ClimateAction action{};
  enums::ClimateFanMode fan_mode{};
  enums::ClimateSwingMode swing_mode{};
  StringRef custom_fan_mode_ref_{};
  void set_custom_fan_mode(const StringRef &ref) { this->custom_fan_mode_ref_ = ref; }
  enums::ClimatePreset preset{};
  StringRef custom_preset_ref_{};
  void set_custom_preset(const StringRef &ref) { this->custom_preset_ref_ = ref; }
  float current_humidity{0.0f};
  float target_humidity{0.0f};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ClimateCommandRequest final : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 48;
  static constexpr uint8_t ESTIMATED_SIZE = 84;
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
class ListEntitiesNumberResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 49;
  static constexpr uint8_t ESTIMATED_SIZE = 75;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_number_response"; }
#endif
  float min_value{0.0f};
  float max_value{0.0f};
  float step{0.0f};
  StringRef unit_of_measurement_ref_{};
  void set_unit_of_measurement(const StringRef &ref) { this->unit_of_measurement_ref_ = ref; }
  enums::NumberMode mode{};
  StringRef device_class_ref_{};
  void set_device_class(const StringRef &ref) { this->device_class_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class NumberStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 50;
  static constexpr uint8_t ESTIMATED_SIZE = 16;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "number_state_response"; }
#endif
  float state{0.0f};
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class NumberCommandRequest final : public CommandProtoMessage {
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
class ListEntitiesSelectResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 52;
  static constexpr uint8_t ESTIMATED_SIZE = 58;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_select_response"; }
#endif
  const FixedVector<const char *> *options{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SelectStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 53;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "select_state_response"; }
#endif
  StringRef state_ref_{};
  void set_state(const StringRef &ref) { this->state_ref_ = ref; }
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SelectCommandRequest final : public CommandProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 54;
  static constexpr uint8_t ESTIMATED_SIZE = 28;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "select_command_request"; }
#endif
  const uint8_t *state{nullptr};
  uint16_t state_len{0};
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
class ListEntitiesSirenResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 55;
  static constexpr uint8_t ESTIMATED_SIZE = 62;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_siren_response"; }
#endif
  std::vector<std::string> tones{};
  bool supports_duration{false};
  bool supports_volume{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SirenStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 56;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "siren_state_response"; }
#endif
  bool state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SirenCommandRequest final : public CommandProtoMessage {
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
class ListEntitiesLockResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 58;
  static constexpr uint8_t ESTIMATED_SIZE = 55;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_lock_response"; }
#endif
  bool assumed_state{false};
  bool supports_open{false};
  bool requires_code{false};
  StringRef code_format_ref_{};
  void set_code_format(const StringRef &ref) { this->code_format_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class LockStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 59;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "lock_state_response"; }
#endif
  enums::LockState state{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class LockCommandRequest final : public CommandProtoMessage {
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
class ListEntitiesButtonResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 61;
  static constexpr uint8_t ESTIMATED_SIZE = 49;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_button_response"; }
#endif
  StringRef device_class_ref_{};
  void set_device_class(const StringRef &ref) { this->device_class_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ButtonCommandRequest final : public CommandProtoMessage {
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
class MediaPlayerSupportedFormat final : public ProtoMessage {
 public:
  StringRef format_ref_{};
  void set_format(const StringRef &ref) { this->format_ref_ = ref; }
  uint32_t sample_rate{0};
  uint32_t num_channels{0};
  enums::MediaPlayerFormatPurpose purpose{};
  uint32_t sample_bytes{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ListEntitiesMediaPlayerResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 63;
  static constexpr uint8_t ESTIMATED_SIZE = 80;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_media_player_response"; }
#endif
  bool supports_pause{false};
  std::vector<MediaPlayerSupportedFormat> supported_formats{};
  uint32_t feature_flags{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class MediaPlayerStateResponse final : public StateResponseProtoMessage {
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
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class MediaPlayerCommandRequest final : public CommandProtoMessage {
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
class SubscribeBluetoothLEAdvertisementsRequest final : public ProtoDecodableMessage {
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
class BluetoothLERawAdvertisement final : public ProtoMessage {
 public:
  uint64_t address{0};
  int32_t rssi{0};
  uint32_t address_type{0};
  uint8_t data[62]{};
  uint8_t data_len{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothLERawAdvertisementsResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 93;
  static constexpr uint8_t ESTIMATED_SIZE = 136;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_le_raw_advertisements_response"; }
#endif
  std::array<BluetoothLERawAdvertisement, BLUETOOTH_PROXY_ADVERTISEMENT_BATCH_SIZE> advertisements{};
  uint16_t advertisements_len{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothDeviceRequest final : public ProtoDecodableMessage {
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
class BluetoothDeviceConnectionResponse final : public ProtoMessage {
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
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTGetServicesRequest final : public ProtoDecodableMessage {
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
class BluetoothGATTDescriptor final : public ProtoMessage {
 public:
  std::array<uint64_t, 2> uuid{};
  uint32_t handle{0};
  uint32_t short_uuid{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTCharacteristic final : public ProtoMessage {
 public:
  std::array<uint64_t, 2> uuid{};
  uint32_t handle{0};
  uint32_t properties{0};
  FixedVector<BluetoothGATTDescriptor> descriptors{};
  uint32_t short_uuid{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTService final : public ProtoMessage {
 public:
  std::array<uint64_t, 2> uuid{};
  uint32_t handle{0};
  FixedVector<BluetoothGATTCharacteristic> characteristics{};
  uint32_t short_uuid{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTGetServicesResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 71;
  static constexpr uint8_t ESTIMATED_SIZE = 38;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_get_services_response"; }
#endif
  uint64_t address{0};
  std::vector<BluetoothGATTService> services{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTGetServicesDoneResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 72;
  static constexpr uint8_t ESTIMATED_SIZE = 4;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_get_services_done_response"; }
#endif
  uint64_t address{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTReadRequest final : public ProtoDecodableMessage {
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
class BluetoothGATTReadResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 74;
  static constexpr uint8_t ESTIMATED_SIZE = 17;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_read_response"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  const uint8_t *data_ptr_{nullptr};
  size_t data_len_{0};
  void set_data(const uint8_t *data, size_t len) {
    this->data_ptr_ = data;
    this->data_len_ = len;
  }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTWriteRequest final : public ProtoDecodableMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 75;
  static constexpr uint8_t ESTIMATED_SIZE = 29;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_write_request"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  bool response{false};
  const uint8_t *data{nullptr};
  uint16_t data_len{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothGATTReadDescriptorRequest final : public ProtoDecodableMessage {
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
class BluetoothGATTWriteDescriptorRequest final : public ProtoDecodableMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 77;
  static constexpr uint8_t ESTIMATED_SIZE = 27;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_write_descriptor_request"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  const uint8_t *data{nullptr};
  uint16_t data_len{0};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class BluetoothGATTNotifyRequest final : public ProtoDecodableMessage {
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
class BluetoothGATTNotifyDataResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 79;
  static constexpr uint8_t ESTIMATED_SIZE = 17;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_notify_data_response"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  const uint8_t *data_ptr_{nullptr};
  size_t data_len_{0};
  void set_data(const uint8_t *data, size_t len) {
    this->data_ptr_ = data;
    this->data_len_ = len;
  }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class SubscribeBluetoothConnectionsFreeRequest final : public ProtoMessage {
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
class BluetoothConnectionsFreeResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 81;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_connections_free_response"; }
#endif
  uint32_t free{0};
  uint32_t limit{0};
  std::array<uint64_t, BLUETOOTH_PROXY_MAX_CONNECTIONS> allocated{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTErrorResponse final : public ProtoMessage {
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
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTWriteResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 83;
  static constexpr uint8_t ESTIMATED_SIZE = 8;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_write_response"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothGATTNotifyResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 84;
  static constexpr uint8_t ESTIMATED_SIZE = 8;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_gatt_notify_response"; }
#endif
  uint64_t address{0};
  uint32_t handle{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothDevicePairingResponse final : public ProtoMessage {
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
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothDeviceUnpairingResponse final : public ProtoMessage {
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
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class UnsubscribeBluetoothLEAdvertisementsRequest final : public ProtoMessage {
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
class BluetoothDeviceClearCacheResponse final : public ProtoMessage {
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
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothScannerStateResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 126;
  static constexpr uint8_t ESTIMATED_SIZE = 6;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "bluetooth_scanner_state_response"; }
#endif
  enums::BluetoothScannerState state{};
  enums::BluetoothScannerMode mode{};
  enums::BluetoothScannerMode configured_mode{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class BluetoothScannerSetModeRequest final : public ProtoDecodableMessage {
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
class SubscribeVoiceAssistantRequest final : public ProtoDecodableMessage {
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
class VoiceAssistantAudioSettings final : public ProtoMessage {
 public:
  uint32_t noise_suppression_level{0};
  uint32_t auto_gain{0};
  float volume_multiplier{0.0f};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class VoiceAssistantRequest final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 90;
  static constexpr uint8_t ESTIMATED_SIZE = 41;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_request"; }
#endif
  bool start{false};
  StringRef conversation_id_ref_{};
  void set_conversation_id(const StringRef &ref) { this->conversation_id_ref_ = ref; }
  uint32_t flags{0};
  VoiceAssistantAudioSettings audio_settings{};
  StringRef wake_word_phrase_ref_{};
  void set_wake_word_phrase(const StringRef &ref) { this->wake_word_phrase_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class VoiceAssistantResponse final : public ProtoDecodableMessage {
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
class VoiceAssistantEventData final : public ProtoDecodableMessage {
 public:
  std::string name{};
  std::string value{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class VoiceAssistantEventResponse final : public ProtoDecodableMessage {
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
class VoiceAssistantAudio final : public ProtoDecodableMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 106;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_audio"; }
#endif
  std::string data{};
  const uint8_t *data_ptr_{nullptr};
  size_t data_len_{0};
  void set_data(const uint8_t *data, size_t len) {
    this->data_ptr_ = data;
    this->data_len_ = len;
  }
  bool end{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class VoiceAssistantTimerEventResponse final : public ProtoDecodableMessage {
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
class VoiceAssistantAnnounceRequest final : public ProtoDecodableMessage {
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
class VoiceAssistantAnnounceFinished final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 120;
  static constexpr uint8_t ESTIMATED_SIZE = 2;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_announce_finished"; }
#endif
  bool success{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class VoiceAssistantWakeWord final : public ProtoMessage {
 public:
  StringRef id_ref_{};
  void set_id(const StringRef &ref) { this->id_ref_ = ref; }
  StringRef wake_word_ref_{};
  void set_wake_word(const StringRef &ref) { this->wake_word_ref_ = ref; }
  std::vector<std::string> trained_languages{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class VoiceAssistantExternalWakeWord final : public ProtoDecodableMessage {
 public:
  std::string id{};
  std::string wake_word{};
  std::vector<std::string> trained_languages{};
  std::string model_type{};
  uint32_t model_size{0};
  std::string model_hash{};
  std::string url{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
class VoiceAssistantConfigurationRequest final : public ProtoDecodableMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 121;
  static constexpr uint8_t ESTIMATED_SIZE = 34;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_configuration_request"; }
#endif
  std::vector<VoiceAssistantExternalWakeWord> external_wake_words{};
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class VoiceAssistantConfigurationResponse final : public ProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 122;
  static constexpr uint8_t ESTIMATED_SIZE = 56;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "voice_assistant_configuration_response"; }
#endif
  std::vector<VoiceAssistantWakeWord> available_wake_words{};
  const std::vector<std::string> *active_wake_words{};
  uint32_t max_active_wake_words{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class VoiceAssistantSetConfiguration final : public ProtoDecodableMessage {
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
class ListEntitiesAlarmControlPanelResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 94;
  static constexpr uint8_t ESTIMATED_SIZE = 48;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_alarm_control_panel_response"; }
#endif
  uint32_t supported_features{0};
  bool requires_code{false};
  bool requires_code_to_arm{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class AlarmControlPanelStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 95;
  static constexpr uint8_t ESTIMATED_SIZE = 11;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "alarm_control_panel_state_response"; }
#endif
  enums::AlarmControlPanelState state{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class AlarmControlPanelCommandRequest final : public CommandProtoMessage {
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
class ListEntitiesTextResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 97;
  static constexpr uint8_t ESTIMATED_SIZE = 59;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_text_response"; }
#endif
  uint32_t min_length{0};
  uint32_t max_length{0};
  StringRef pattern_ref_{};
  void set_pattern(const StringRef &ref) { this->pattern_ref_ = ref; }
  enums::TextMode mode{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class TextStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 98;
  static constexpr uint8_t ESTIMATED_SIZE = 20;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "text_state_response"; }
#endif
  StringRef state_ref_{};
  void set_state(const StringRef &ref) { this->state_ref_ = ref; }
  bool missing_state{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class TextCommandRequest final : public CommandProtoMessage {
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
class ListEntitiesDateResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 100;
  static constexpr uint8_t ESTIMATED_SIZE = 40;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_date_response"; }
#endif
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class DateStateResponse final : public StateResponseProtoMessage {
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
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class DateCommandRequest final : public CommandProtoMessage {
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
class ListEntitiesTimeResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 103;
  static constexpr uint8_t ESTIMATED_SIZE = 40;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_time_response"; }
#endif
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class TimeStateResponse final : public StateResponseProtoMessage {
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
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class TimeCommandRequest final : public CommandProtoMessage {
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
class ListEntitiesEventResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 107;
  static constexpr uint8_t ESTIMATED_SIZE = 67;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_event_response"; }
#endif
  StringRef device_class_ref_{};
  void set_device_class(const StringRef &ref) { this->device_class_ref_ = ref; }
  const FixedVector<const char *> *event_types{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class EventResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 108;
  static constexpr uint8_t ESTIMATED_SIZE = 18;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "event_response"; }
#endif
  StringRef event_type_ref_{};
  void set_event_type(const StringRef &ref) { this->event_type_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
#endif
#ifdef USE_VALVE
class ListEntitiesValveResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 109;
  static constexpr uint8_t ESTIMATED_SIZE = 55;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_valve_response"; }
#endif
  StringRef device_class_ref_{};
  void set_device_class(const StringRef &ref) { this->device_class_ref_ = ref; }
  bool assumed_state{false};
  bool supports_position{false};
  bool supports_stop{false};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ValveStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 110;
  static constexpr uint8_t ESTIMATED_SIZE = 16;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "valve_state_response"; }
#endif
  float position{0.0f};
  enums::ValveOperation current_operation{};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class ValveCommandRequest final : public CommandProtoMessage {
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
class ListEntitiesDateTimeResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 112;
  static constexpr uint8_t ESTIMATED_SIZE = 40;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_date_time_response"; }
#endif
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class DateTimeStateResponse final : public StateResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 113;
  static constexpr uint8_t ESTIMATED_SIZE = 16;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "date_time_state_response"; }
#endif
  bool missing_state{false};
  uint32_t epoch_seconds{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class DateTimeCommandRequest final : public CommandProtoMessage {
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
class ListEntitiesUpdateResponse final : public InfoResponseProtoMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 116;
  static constexpr uint8_t ESTIMATED_SIZE = 49;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "list_entities_update_response"; }
#endif
  StringRef device_class_ref_{};
  void set_device_class(const StringRef &ref) { this->device_class_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class UpdateStateResponse final : public StateResponseProtoMessage {
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
  StringRef current_version_ref_{};
  void set_current_version(const StringRef &ref) { this->current_version_ref_ = ref; }
  StringRef latest_version_ref_{};
  void set_latest_version(const StringRef &ref) { this->latest_version_ref_ = ref; }
  StringRef title_ref_{};
  void set_title(const StringRef &ref) { this->title_ref_ = ref; }
  StringRef release_summary_ref_{};
  void set_release_summary(const StringRef &ref) { this->release_summary_ref_ = ref; }
  StringRef release_url_ref_{};
  void set_release_url(const StringRef &ref) { this->release_url_ref_ = ref; }
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
};
class UpdateCommandRequest final : public CommandProtoMessage {
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
#ifdef USE_ZWAVE_PROXY
class ZWaveProxyFrame final : public ProtoDecodableMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 128;
  static constexpr uint8_t ESTIMATED_SIZE = 19;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "z_wave_proxy_frame"; }
#endif
  const uint8_t *data{nullptr};
  uint16_t data_len{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
};
class ZWaveProxyRequest final : public ProtoDecodableMessage {
 public:
  static constexpr uint8_t MESSAGE_TYPE = 129;
  static constexpr uint8_t ESTIMATED_SIZE = 21;
#ifdef HAS_PROTO_MESSAGE_DUMP
  const char *message_name() const override { return "z_wave_proxy_request"; }
#endif
  enums::ZWaveProxyRequestType type{};
  const uint8_t *data{nullptr};
  uint16_t data_len{0};
  void encode(ProtoWriteBuffer buffer) const override;
  void calculate_size(ProtoSize &size) const override;
#ifdef HAS_PROTO_MESSAGE_DUMP
  void dump_to(std::string &out) const override;
#endif

 protected:
  bool decode_length(uint32_t field_id, ProtoLengthDelimited value) override;
  bool decode_varint(uint32_t field_id, ProtoVarInt value) override;
};
#endif

}  // namespace esphome::api
