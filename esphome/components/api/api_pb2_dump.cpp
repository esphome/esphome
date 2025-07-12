// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#include "api_pb2.h"
#include "esphome/core/helpers.h"

#include <cinttypes>

#ifdef HAS_PROTO_MESSAGE_DUMP

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
#ifdef USE_COVER
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
#endif
#ifdef USE_FAN
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
#endif
#ifdef USE_LIGHT
template<> const char *proto_enum_to_string<enums::ColorMode>(enums::ColorMode value) {
  switch (value) {
    case enums::COLOR_MODE_UNKNOWN:
      return "COLOR_MODE_UNKNOWN";
    case enums::COLOR_MODE_ON_OFF:
      return "COLOR_MODE_ON_OFF";
    case enums::COLOR_MODE_LEGACY_BRIGHTNESS:
      return "COLOR_MODE_LEGACY_BRIGHTNESS";
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
#endif
#ifdef USE_SENSOR
template<> const char *proto_enum_to_string<enums::SensorStateClass>(enums::SensorStateClass value) {
  switch (value) {
    case enums::STATE_CLASS_NONE:
      return "STATE_CLASS_NONE";
    case enums::STATE_CLASS_MEASUREMENT:
      return "STATE_CLASS_MEASUREMENT";
    case enums::STATE_CLASS_TOTAL_INCREASING:
      return "STATE_CLASS_TOTAL_INCREASING";
    case enums::STATE_CLASS_TOTAL:
      return "STATE_CLASS_TOTAL";
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
#endif
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
#ifdef USE_API_SERVICES
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
#endif
#ifdef USE_CLIMATE
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
    case enums::CLIMATE_FAN_QUIET:
      return "CLIMATE_FAN_QUIET";
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
#endif
#ifdef USE_NUMBER
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
#endif
#ifdef USE_LOCK
template<> const char *proto_enum_to_string<enums::LockState>(enums::LockState value) {
  switch (value) {
    case enums::LOCK_STATE_NONE:
      return "LOCK_STATE_NONE";
    case enums::LOCK_STATE_LOCKED:
      return "LOCK_STATE_LOCKED";
    case enums::LOCK_STATE_UNLOCKED:
      return "LOCK_STATE_UNLOCKED";
    case enums::LOCK_STATE_JAMMED:
      return "LOCK_STATE_JAMMED";
    case enums::LOCK_STATE_LOCKING:
      return "LOCK_STATE_LOCKING";
    case enums::LOCK_STATE_UNLOCKING:
      return "LOCK_STATE_UNLOCKING";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::LockCommand>(enums::LockCommand value) {
  switch (value) {
    case enums::LOCK_UNLOCK:
      return "LOCK_UNLOCK";
    case enums::LOCK_LOCK:
      return "LOCK_LOCK";
    case enums::LOCK_OPEN:
      return "LOCK_OPEN";
    default:
      return "UNKNOWN";
  }
}
#endif
#ifdef USE_MEDIA_PLAYER
template<> const char *proto_enum_to_string<enums::MediaPlayerState>(enums::MediaPlayerState value) {
  switch (value) {
    case enums::MEDIA_PLAYER_STATE_NONE:
      return "MEDIA_PLAYER_STATE_NONE";
    case enums::MEDIA_PLAYER_STATE_IDLE:
      return "MEDIA_PLAYER_STATE_IDLE";
    case enums::MEDIA_PLAYER_STATE_PLAYING:
      return "MEDIA_PLAYER_STATE_PLAYING";
    case enums::MEDIA_PLAYER_STATE_PAUSED:
      return "MEDIA_PLAYER_STATE_PAUSED";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::MediaPlayerCommand>(enums::MediaPlayerCommand value) {
  switch (value) {
    case enums::MEDIA_PLAYER_COMMAND_PLAY:
      return "MEDIA_PLAYER_COMMAND_PLAY";
    case enums::MEDIA_PLAYER_COMMAND_PAUSE:
      return "MEDIA_PLAYER_COMMAND_PAUSE";
    case enums::MEDIA_PLAYER_COMMAND_STOP:
      return "MEDIA_PLAYER_COMMAND_STOP";
    case enums::MEDIA_PLAYER_COMMAND_MUTE:
      return "MEDIA_PLAYER_COMMAND_MUTE";
    case enums::MEDIA_PLAYER_COMMAND_UNMUTE:
      return "MEDIA_PLAYER_COMMAND_UNMUTE";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::MediaPlayerFormatPurpose>(enums::MediaPlayerFormatPurpose value) {
  switch (value) {
    case enums::MEDIA_PLAYER_FORMAT_PURPOSE_DEFAULT:
      return "MEDIA_PLAYER_FORMAT_PURPOSE_DEFAULT";
    case enums::MEDIA_PLAYER_FORMAT_PURPOSE_ANNOUNCEMENT:
      return "MEDIA_PLAYER_FORMAT_PURPOSE_ANNOUNCEMENT";
    default:
      return "UNKNOWN";
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
template<>
const char *proto_enum_to_string<enums::BluetoothDeviceRequestType>(enums::BluetoothDeviceRequestType value) {
  switch (value) {
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT:
      return "BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT";
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_DISCONNECT:
      return "BLUETOOTH_DEVICE_REQUEST_TYPE_DISCONNECT";
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR:
      return "BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR";
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR:
      return "BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR";
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE:
      return "BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE";
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE:
      return "BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE";
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CLEAR_CACHE:
      return "BLUETOOTH_DEVICE_REQUEST_TYPE_CLEAR_CACHE";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::BluetoothScannerState>(enums::BluetoothScannerState value) {
  switch (value) {
    case enums::BLUETOOTH_SCANNER_STATE_IDLE:
      return "BLUETOOTH_SCANNER_STATE_IDLE";
    case enums::BLUETOOTH_SCANNER_STATE_STARTING:
      return "BLUETOOTH_SCANNER_STATE_STARTING";
    case enums::BLUETOOTH_SCANNER_STATE_RUNNING:
      return "BLUETOOTH_SCANNER_STATE_RUNNING";
    case enums::BLUETOOTH_SCANNER_STATE_FAILED:
      return "BLUETOOTH_SCANNER_STATE_FAILED";
    case enums::BLUETOOTH_SCANNER_STATE_STOPPING:
      return "BLUETOOTH_SCANNER_STATE_STOPPING";
    case enums::BLUETOOTH_SCANNER_STATE_STOPPED:
      return "BLUETOOTH_SCANNER_STATE_STOPPED";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::BluetoothScannerMode>(enums::BluetoothScannerMode value) {
  switch (value) {
    case enums::BLUETOOTH_SCANNER_MODE_PASSIVE:
      return "BLUETOOTH_SCANNER_MODE_PASSIVE";
    case enums::BLUETOOTH_SCANNER_MODE_ACTIVE:
      return "BLUETOOTH_SCANNER_MODE_ACTIVE";
    default:
      return "UNKNOWN";
  }
}
#endif
template<>
const char *proto_enum_to_string<enums::VoiceAssistantSubscribeFlag>(enums::VoiceAssistantSubscribeFlag value) {
  switch (value) {
    case enums::VOICE_ASSISTANT_SUBSCRIBE_NONE:
      return "VOICE_ASSISTANT_SUBSCRIBE_NONE";
    case enums::VOICE_ASSISTANT_SUBSCRIBE_API_AUDIO:
      return "VOICE_ASSISTANT_SUBSCRIBE_API_AUDIO";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::VoiceAssistantRequestFlag>(enums::VoiceAssistantRequestFlag value) {
  switch (value) {
    case enums::VOICE_ASSISTANT_REQUEST_NONE:
      return "VOICE_ASSISTANT_REQUEST_NONE";
    case enums::VOICE_ASSISTANT_REQUEST_USE_VAD:
      return "VOICE_ASSISTANT_REQUEST_USE_VAD";
    case enums::VOICE_ASSISTANT_REQUEST_USE_WAKE_WORD:
      return "VOICE_ASSISTANT_REQUEST_USE_WAKE_WORD";
    default:
      return "UNKNOWN";
  }
}
#ifdef USE_VOICE_ASSISTANT
template<> const char *proto_enum_to_string<enums::VoiceAssistantEvent>(enums::VoiceAssistantEvent value) {
  switch (value) {
    case enums::VOICE_ASSISTANT_ERROR:
      return "VOICE_ASSISTANT_ERROR";
    case enums::VOICE_ASSISTANT_RUN_START:
      return "VOICE_ASSISTANT_RUN_START";
    case enums::VOICE_ASSISTANT_RUN_END:
      return "VOICE_ASSISTANT_RUN_END";
    case enums::VOICE_ASSISTANT_STT_START:
      return "VOICE_ASSISTANT_STT_START";
    case enums::VOICE_ASSISTANT_STT_END:
      return "VOICE_ASSISTANT_STT_END";
    case enums::VOICE_ASSISTANT_INTENT_START:
      return "VOICE_ASSISTANT_INTENT_START";
    case enums::VOICE_ASSISTANT_INTENT_END:
      return "VOICE_ASSISTANT_INTENT_END";
    case enums::VOICE_ASSISTANT_TTS_START:
      return "VOICE_ASSISTANT_TTS_START";
    case enums::VOICE_ASSISTANT_TTS_END:
      return "VOICE_ASSISTANT_TTS_END";
    case enums::VOICE_ASSISTANT_WAKE_WORD_START:
      return "VOICE_ASSISTANT_WAKE_WORD_START";
    case enums::VOICE_ASSISTANT_WAKE_WORD_END:
      return "VOICE_ASSISTANT_WAKE_WORD_END";
    case enums::VOICE_ASSISTANT_STT_VAD_START:
      return "VOICE_ASSISTANT_STT_VAD_START";
    case enums::VOICE_ASSISTANT_STT_VAD_END:
      return "VOICE_ASSISTANT_STT_VAD_END";
    case enums::VOICE_ASSISTANT_TTS_STREAM_START:
      return "VOICE_ASSISTANT_TTS_STREAM_START";
    case enums::VOICE_ASSISTANT_TTS_STREAM_END:
      return "VOICE_ASSISTANT_TTS_STREAM_END";
    case enums::VOICE_ASSISTANT_INTENT_PROGRESS:
      return "VOICE_ASSISTANT_INTENT_PROGRESS";
    default:
      return "UNKNOWN";
  }
}
template<> const char *proto_enum_to_string<enums::VoiceAssistantTimerEvent>(enums::VoiceAssistantTimerEvent value) {
  switch (value) {
    case enums::VOICE_ASSISTANT_TIMER_STARTED:
      return "VOICE_ASSISTANT_TIMER_STARTED";
    case enums::VOICE_ASSISTANT_TIMER_UPDATED:
      return "VOICE_ASSISTANT_TIMER_UPDATED";
    case enums::VOICE_ASSISTANT_TIMER_CANCELLED:
      return "VOICE_ASSISTANT_TIMER_CANCELLED";
    case enums::VOICE_ASSISTANT_TIMER_FINISHED:
      return "VOICE_ASSISTANT_TIMER_FINISHED";
    default:
      return "UNKNOWN";
  }
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
template<> const char *proto_enum_to_string<enums::AlarmControlPanelState>(enums::AlarmControlPanelState value) {
  switch (value) {
    case enums::ALARM_STATE_DISARMED:
      return "ALARM_STATE_DISARMED";
    case enums::ALARM_STATE_ARMED_HOME:
      return "ALARM_STATE_ARMED_HOME";
    case enums::ALARM_STATE_ARMED_AWAY:
      return "ALARM_STATE_ARMED_AWAY";
    case enums::ALARM_STATE_ARMED_NIGHT:
      return "ALARM_STATE_ARMED_NIGHT";
    case enums::ALARM_STATE_ARMED_VACATION:
      return "ALARM_STATE_ARMED_VACATION";
    case enums::ALARM_STATE_ARMED_CUSTOM_BYPASS:
      return "ALARM_STATE_ARMED_CUSTOM_BYPASS";
    case enums::ALARM_STATE_PENDING:
      return "ALARM_STATE_PENDING";
    case enums::ALARM_STATE_ARMING:
      return "ALARM_STATE_ARMING";
    case enums::ALARM_STATE_DISARMING:
      return "ALARM_STATE_DISARMING";
    case enums::ALARM_STATE_TRIGGERED:
      return "ALARM_STATE_TRIGGERED";
    default:
      return "UNKNOWN";
  }
}
template<>
const char *proto_enum_to_string<enums::AlarmControlPanelStateCommand>(enums::AlarmControlPanelStateCommand value) {
  switch (value) {
    case enums::ALARM_CONTROL_PANEL_DISARM:
      return "ALARM_CONTROL_PANEL_DISARM";
    case enums::ALARM_CONTROL_PANEL_ARM_AWAY:
      return "ALARM_CONTROL_PANEL_ARM_AWAY";
    case enums::ALARM_CONTROL_PANEL_ARM_HOME:
      return "ALARM_CONTROL_PANEL_ARM_HOME";
    case enums::ALARM_CONTROL_PANEL_ARM_NIGHT:
      return "ALARM_CONTROL_PANEL_ARM_NIGHT";
    case enums::ALARM_CONTROL_PANEL_ARM_VACATION:
      return "ALARM_CONTROL_PANEL_ARM_VACATION";
    case enums::ALARM_CONTROL_PANEL_ARM_CUSTOM_BYPASS:
      return "ALARM_CONTROL_PANEL_ARM_CUSTOM_BYPASS";
    case enums::ALARM_CONTROL_PANEL_TRIGGER:
      return "ALARM_CONTROL_PANEL_TRIGGER";
    default:
      return "UNKNOWN";
  }
}
#endif
#ifdef USE_TEXT
template<> const char *proto_enum_to_string<enums::TextMode>(enums::TextMode value) {
  switch (value) {
    case enums::TEXT_MODE_TEXT:
      return "TEXT_MODE_TEXT";
    case enums::TEXT_MODE_PASSWORD:
      return "TEXT_MODE_PASSWORD";
    default:
      return "UNKNOWN";
  }
}
#endif
#ifdef USE_VALVE
template<> const char *proto_enum_to_string<enums::ValveOperation>(enums::ValveOperation value) {
  switch (value) {
    case enums::VALVE_OPERATION_IDLE:
      return "VALVE_OPERATION_IDLE";
    case enums::VALVE_OPERATION_IS_OPENING:
      return "VALVE_OPERATION_IS_OPENING";
    case enums::VALVE_OPERATION_IS_CLOSING:
      return "VALVE_OPERATION_IS_CLOSING";
    default:
      return "UNKNOWN";
  }
}
#endif
#ifdef USE_UPDATE
template<> const char *proto_enum_to_string<enums::UpdateCommand>(enums::UpdateCommand value) {
  switch (value) {
    case enums::UPDATE_COMMAND_NONE:
      return "UPDATE_COMMAND_NONE";
    case enums::UPDATE_COMMAND_UPDATE:
      return "UPDATE_COMMAND_UPDATE";
    case enums::UPDATE_COMMAND_CHECK:
      return "UPDATE_COMMAND_CHECK";
    default:
      return "UNKNOWN";
  }
}
#endif

void HelloRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("HelloRequest {\n");
  out.append("  client_info: ");
  out.append("'").append(this->client_info).append("'");
  out.append("\n");

  out.append("  api_version_major: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->api_version_major);
  out.append(buffer);
  out.append("\n");

  out.append("  api_version_minor: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->api_version_minor);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void HelloResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("HelloResponse {\n");
  out.append("  api_version_major: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->api_version_major);
  out.append(buffer);
  out.append("\n");

  out.append("  api_version_minor: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->api_version_minor);
  out.append(buffer);
  out.append("\n");

  out.append("  server_info: ");
  out.append("'").append(this->server_info).append("'");
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");
  out.append("}");
}
void ConnectRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ConnectRequest {\n");
  out.append("  password: ");
  out.append("'").append(this->password).append("'");
  out.append("\n");
  out.append("}");
}
void ConnectResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ConnectResponse {\n");
  out.append("  invalid_password: ");
  out.append(YESNO(this->invalid_password));
  out.append("\n");
  out.append("}");
}
void DisconnectRequest::dump_to(std::string &out) const { out.append("DisconnectRequest {}"); }
void DisconnectResponse::dump_to(std::string &out) const { out.append("DisconnectResponse {}"); }
void PingRequest::dump_to(std::string &out) const { out.append("PingRequest {}"); }
void PingResponse::dump_to(std::string &out) const { out.append("PingResponse {}"); }
void DeviceInfoRequest::dump_to(std::string &out) const { out.append("DeviceInfoRequest {}"); }
void AreaInfo::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("AreaInfo {\n");
  out.append("  area_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->area_id);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");
  out.append("}");
}
void DeviceInfo::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("DeviceInfo {\n");
  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  area_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->area_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
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
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->webserver_port);
  out.append(buffer);
  out.append("\n");

  out.append("  legacy_bluetooth_proxy_version: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->legacy_bluetooth_proxy_version);
  out.append(buffer);
  out.append("\n");

  out.append("  bluetooth_proxy_feature_flags: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->bluetooth_proxy_feature_flags);
  out.append(buffer);
  out.append("\n");

  out.append("  manufacturer: ");
  out.append("'").append(this->manufacturer).append("'");
  out.append("\n");

  out.append("  friendly_name: ");
  out.append("'").append(this->friendly_name).append("'");
  out.append("\n");

  out.append("  legacy_voice_assistant_version: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->legacy_voice_assistant_version);
  out.append(buffer);
  out.append("\n");

  out.append("  voice_assistant_feature_flags: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->voice_assistant_feature_flags);
  out.append(buffer);
  out.append("\n");

  out.append("  suggested_area: ");
  out.append("'").append(this->suggested_area).append("'");
  out.append("\n");

  out.append("  bluetooth_mac_address: ");
  out.append("'").append(this->bluetooth_mac_address).append("'");
  out.append("\n");

  out.append("  api_encryption_supported: ");
  out.append(YESNO(this->api_encryption_supported));
  out.append("\n");

  for (const auto &it : this->devices) {
    out.append("  devices: ");
    it.dump_to(out);
    out.append("\n");
  }

  for (const auto &it : this->areas) {
    out.append("  areas: ");
    it.dump_to(out);
    out.append("\n");
  }

  out.append("  area: ");
  this->area.dump_to(out);
  out.append("\n");
  out.append("}");
}
void ListEntitiesRequest::dump_to(std::string &out) const { out.append("ListEntitiesRequest {}"); }
void ListEntitiesDoneResponse::dump_to(std::string &out) const { out.append("ListEntitiesDoneResponse {}"); }
void SubscribeStatesRequest::dump_to(std::string &out) const { out.append("SubscribeStatesRequest {}"); }
#ifdef USE_BINARY_SENSOR
void ListEntitiesBinarySensorResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesBinarySensorResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BinarySensorStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BinarySensorStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_COVER
void ListEntitiesCoverResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesCoverResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  supports_stop: ");
  out.append(YESNO(this->supports_stop));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void CoverStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("CoverStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  legacy_state: ");
  out.append(proto_enum_to_string<enums::LegacyCoverState>(this->legacy_state));
  out.append("\n");

  out.append("  position: ");
  snprintf(buffer, sizeof(buffer), "%g", this->position);
  out.append(buffer);
  out.append("\n");

  out.append("  tilt: ");
  snprintf(buffer, sizeof(buffer), "%g", this->tilt);
  out.append(buffer);
  out.append("\n");

  out.append("  current_operation: ");
  out.append(proto_enum_to_string<enums::CoverOperation>(this->current_operation));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void CoverCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("CoverCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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
  snprintf(buffer, sizeof(buffer), "%g", this->position);
  out.append(buffer);
  out.append("\n");

  out.append("  has_tilt: ");
  out.append(YESNO(this->has_tilt));
  out.append("\n");

  out.append("  tilt: ");
  snprintf(buffer, sizeof(buffer), "%g", this->tilt);
  out.append(buffer);
  out.append("\n");

  out.append("  stop: ");
  out.append(YESNO(this->stop));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_FAN
void ListEntitiesFanResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesFanResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->supported_speed_count);
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

  for (const auto &it : this->supported_preset_modes) {
    out.append("  supported_preset_modes: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void FanStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("FanStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->speed_level);
  out.append(buffer);
  out.append("\n");

  out.append("  preset_mode: ");
  out.append("'").append(this->preset_mode).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void FanCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("FanCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->speed_level);
  out.append(buffer);
  out.append("\n");

  out.append("  has_preset_mode: ");
  out.append(YESNO(this->has_preset_mode));
  out.append("\n");

  out.append("  preset_mode: ");
  out.append("'").append(this->preset_mode).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_LIGHT
void ListEntitiesLightResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesLightResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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
  snprintf(buffer, sizeof(buffer), "%g", this->min_mireds);
  out.append(buffer);
  out.append("\n");

  out.append("  max_mireds: ");
  snprintf(buffer, sizeof(buffer), "%g", this->max_mireds);
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

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void LightStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("LightStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");

  out.append("  brightness: ");
  snprintf(buffer, sizeof(buffer), "%g", this->brightness);
  out.append(buffer);
  out.append("\n");

  out.append("  color_mode: ");
  out.append(proto_enum_to_string<enums::ColorMode>(this->color_mode));
  out.append("\n");

  out.append("  color_brightness: ");
  snprintf(buffer, sizeof(buffer), "%g", this->color_brightness);
  out.append(buffer);
  out.append("\n");

  out.append("  red: ");
  snprintf(buffer, sizeof(buffer), "%g", this->red);
  out.append(buffer);
  out.append("\n");

  out.append("  green: ");
  snprintf(buffer, sizeof(buffer), "%g", this->green);
  out.append(buffer);
  out.append("\n");

  out.append("  blue: ");
  snprintf(buffer, sizeof(buffer), "%g", this->blue);
  out.append(buffer);
  out.append("\n");

  out.append("  white: ");
  snprintf(buffer, sizeof(buffer), "%g", this->white);
  out.append(buffer);
  out.append("\n");

  out.append("  color_temperature: ");
  snprintf(buffer, sizeof(buffer), "%g", this->color_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  cold_white: ");
  snprintf(buffer, sizeof(buffer), "%g", this->cold_white);
  out.append(buffer);
  out.append("\n");

  out.append("  warm_white: ");
  snprintf(buffer, sizeof(buffer), "%g", this->warm_white);
  out.append(buffer);
  out.append("\n");

  out.append("  effect: ");
  out.append("'").append(this->effect).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void LightCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("LightCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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
  snprintf(buffer, sizeof(buffer), "%g", this->brightness);
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
  snprintf(buffer, sizeof(buffer), "%g", this->color_brightness);
  out.append(buffer);
  out.append("\n");

  out.append("  has_rgb: ");
  out.append(YESNO(this->has_rgb));
  out.append("\n");

  out.append("  red: ");
  snprintf(buffer, sizeof(buffer), "%g", this->red);
  out.append(buffer);
  out.append("\n");

  out.append("  green: ");
  snprintf(buffer, sizeof(buffer), "%g", this->green);
  out.append(buffer);
  out.append("\n");

  out.append("  blue: ");
  snprintf(buffer, sizeof(buffer), "%g", this->blue);
  out.append(buffer);
  out.append("\n");

  out.append("  has_white: ");
  out.append(YESNO(this->has_white));
  out.append("\n");

  out.append("  white: ");
  snprintf(buffer, sizeof(buffer), "%g", this->white);
  out.append(buffer);
  out.append("\n");

  out.append("  has_color_temperature: ");
  out.append(YESNO(this->has_color_temperature));
  out.append("\n");

  out.append("  color_temperature: ");
  snprintf(buffer, sizeof(buffer), "%g", this->color_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  has_cold_white: ");
  out.append(YESNO(this->has_cold_white));
  out.append("\n");

  out.append("  cold_white: ");
  snprintf(buffer, sizeof(buffer), "%g", this->cold_white);
  out.append(buffer);
  out.append("\n");

  out.append("  has_warm_white: ");
  out.append(YESNO(this->has_warm_white));
  out.append("\n");

  out.append("  warm_white: ");
  snprintf(buffer, sizeof(buffer), "%g", this->warm_white);
  out.append(buffer);
  out.append("\n");

  out.append("  has_transition_length: ");
  out.append(YESNO(this->has_transition_length));
  out.append("\n");

  out.append("  transition_length: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->transition_length);
  out.append(buffer);
  out.append("\n");

  out.append("  has_flash_length: ");
  out.append(YESNO(this->has_flash_length));
  out.append("\n");

  out.append("  flash_length: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->flash_length);
  out.append(buffer);
  out.append("\n");

  out.append("  has_effect: ");
  out.append(YESNO(this->has_effect));
  out.append("\n");

  out.append("  effect: ");
  out.append("'").append(this->effect).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_SENSOR
void ListEntitiesSensorResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesSensorResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->accuracy_decimals);
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

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void SensorStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SensorStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  snprintf(buffer, sizeof(buffer), "%g", this->state);
  out.append(buffer);
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_SWITCH
void ListEntitiesSwitchResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesSwitchResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  device_class: ");
  out.append("'").append(this->device_class).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void SwitchStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SwitchStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void SwitchCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SwitchCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_TEXT_SENSOR
void ListEntitiesTextSensorResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesTextSensorResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void TextSensorStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("TextSensorStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append("'").append(this->state).append("'");
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
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
void SubscribeLogsResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SubscribeLogsResponse {\n");
  out.append("  level: ");
  out.append(proto_enum_to_string<enums::LogLevel>(this->level));
  out.append("\n");

  out.append("  message: ");
  out.append(format_hex_pretty(this->message));
  out.append("\n");

  out.append("  send_failed: ");
  out.append(YESNO(this->send_failed));
  out.append("\n");
  out.append("}");
}
#ifdef USE_API_NOISE
void NoiseEncryptionSetKeyRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("NoiseEncryptionSetKeyRequest {\n");
  out.append("  key: ");
  out.append(format_hex_pretty(this->key));
  out.append("\n");
  out.append("}");
}
void NoiseEncryptionSetKeyResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("NoiseEncryptionSetKeyResponse {\n");
  out.append("  success: ");
  out.append(YESNO(this->success));
  out.append("\n");
  out.append("}");
}
#endif
void SubscribeHomeassistantServicesRequest::dump_to(std::string &out) const {
  out.append("SubscribeHomeassistantServicesRequest {}");
}
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
void SubscribeHomeAssistantStatesRequest::dump_to(std::string &out) const {
  out.append("SubscribeHomeAssistantStatesRequest {}");
}
void SubscribeHomeAssistantStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SubscribeHomeAssistantStateResponse {\n");
  out.append("  entity_id: ");
  out.append("'").append(this->entity_id).append("'");
  out.append("\n");

  out.append("  attribute: ");
  out.append("'").append(this->attribute).append("'");
  out.append("\n");

  out.append("  once: ");
  out.append(YESNO(this->once));
  out.append("\n");
  out.append("}");
}
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
void GetTimeRequest::dump_to(std::string &out) const { out.append("GetTimeRequest {}"); }
void GetTimeResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("GetTimeResponse {\n");
  out.append("  epoch_seconds: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->epoch_seconds);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#ifdef USE_API_SERVICES
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
void ListEntitiesServicesResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesServicesResponse {\n");
  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  for (const auto &it : this->args) {
    out.append("  args: ");
    it.dump_to(out);
    out.append("\n");
  }
  out.append("}");
}
void ExecuteServiceArgument::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ExecuteServiceArgument {\n");
  out.append("  bool_: ");
  out.append(YESNO(this->bool_));
  out.append("\n");

  out.append("  legacy_int: ");
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->legacy_int);
  out.append(buffer);
  out.append("\n");

  out.append("  float_: ");
  snprintf(buffer, sizeof(buffer), "%g", this->float_);
  out.append(buffer);
  out.append("\n");

  out.append("  string_: ");
  out.append("'").append(this->string_).append("'");
  out.append("\n");

  out.append("  int_: ");
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->int_);
  out.append(buffer);
  out.append("\n");

  for (const auto it : this->bool_array) {
    out.append("  bool_array: ");
    out.append(YESNO(it));
    out.append("\n");
  }

  for (const auto &it : this->int_array) {
    out.append("  int_array: ");
    snprintf(buffer, sizeof(buffer), "%" PRId32, it);
    out.append(buffer);
    out.append("\n");
  }

  for (const auto &it : this->float_array) {
    out.append("  float_array: ");
    snprintf(buffer, sizeof(buffer), "%g", it);
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
void ExecuteServiceRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ExecuteServiceRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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
#ifdef USE_CAMERA
void ListEntitiesCameraResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesCameraResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void CameraImageResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("CameraImageResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  data: ");
  out.append(format_hex_pretty(this->data));
  out.append("\n");

  out.append("  done: ");
  out.append(YESNO(this->done));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
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
#ifdef USE_CLIMATE
void ListEntitiesClimateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesClimateResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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
  snprintf(buffer, sizeof(buffer), "%g", this->visual_min_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  visual_max_temperature: ");
  snprintf(buffer, sizeof(buffer), "%g", this->visual_max_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  visual_target_temperature_step: ");
  snprintf(buffer, sizeof(buffer), "%g", this->visual_target_temperature_step);
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

  out.append("  visual_current_temperature_step: ");
  snprintf(buffer, sizeof(buffer), "%g", this->visual_current_temperature_step);
  out.append(buffer);
  out.append("\n");

  out.append("  supports_current_humidity: ");
  out.append(YESNO(this->supports_current_humidity));
  out.append("\n");

  out.append("  supports_target_humidity: ");
  out.append(YESNO(this->supports_target_humidity));
  out.append("\n");

  out.append("  visual_min_humidity: ");
  snprintf(buffer, sizeof(buffer), "%g", this->visual_min_humidity);
  out.append(buffer);
  out.append("\n");

  out.append("  visual_max_humidity: ");
  snprintf(buffer, sizeof(buffer), "%g", this->visual_max_humidity);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void ClimateStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ClimateStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  mode: ");
  out.append(proto_enum_to_string<enums::ClimateMode>(this->mode));
  out.append("\n");

  out.append("  current_temperature: ");
  snprintf(buffer, sizeof(buffer), "%g", this->current_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  target_temperature: ");
  snprintf(buffer, sizeof(buffer), "%g", this->target_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  target_temperature_low: ");
  snprintf(buffer, sizeof(buffer), "%g", this->target_temperature_low);
  out.append(buffer);
  out.append("\n");

  out.append("  target_temperature_high: ");
  snprintf(buffer, sizeof(buffer), "%g", this->target_temperature_high);
  out.append(buffer);
  out.append("\n");

  out.append("  unused_legacy_away: ");
  out.append(YESNO(this->unused_legacy_away));
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

  out.append("  current_humidity: ");
  snprintf(buffer, sizeof(buffer), "%g", this->current_humidity);
  out.append(buffer);
  out.append("\n");

  out.append("  target_humidity: ");
  snprintf(buffer, sizeof(buffer), "%g", this->target_humidity);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void ClimateCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ClimateCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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
  snprintf(buffer, sizeof(buffer), "%g", this->target_temperature);
  out.append(buffer);
  out.append("\n");

  out.append("  has_target_temperature_low: ");
  out.append(YESNO(this->has_target_temperature_low));
  out.append("\n");

  out.append("  target_temperature_low: ");
  snprintf(buffer, sizeof(buffer), "%g", this->target_temperature_low);
  out.append(buffer);
  out.append("\n");

  out.append("  has_target_temperature_high: ");
  out.append(YESNO(this->has_target_temperature_high));
  out.append("\n");

  out.append("  target_temperature_high: ");
  snprintf(buffer, sizeof(buffer), "%g", this->target_temperature_high);
  out.append(buffer);
  out.append("\n");

  out.append("  unused_has_legacy_away: ");
  out.append(YESNO(this->unused_has_legacy_away));
  out.append("\n");

  out.append("  unused_legacy_away: ");
  out.append(YESNO(this->unused_legacy_away));
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

  out.append("  has_target_humidity: ");
  out.append(YESNO(this->has_target_humidity));
  out.append("\n");

  out.append("  target_humidity: ");
  snprintf(buffer, sizeof(buffer), "%g", this->target_humidity);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_NUMBER
void ListEntitiesNumberResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesNumberResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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
  snprintf(buffer, sizeof(buffer), "%g", this->min_value);
  out.append(buffer);
  out.append("\n");

  out.append("  max_value: ");
  snprintf(buffer, sizeof(buffer), "%g", this->max_value);
  out.append(buffer);
  out.append("\n");

  out.append("  step: ");
  snprintf(buffer, sizeof(buffer), "%g", this->step);
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

  out.append("  device_class: ");
  out.append("'").append(this->device_class).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void NumberStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("NumberStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  snprintf(buffer, sizeof(buffer), "%g", this->state);
  out.append(buffer);
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void NumberCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("NumberCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  snprintf(buffer, sizeof(buffer), "%g", this->state);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_SELECT
void ListEntitiesSelectResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesSelectResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void SelectStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SelectStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append("'").append(this->state).append("'");
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void SelectCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SelectCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append("'").append(this->state).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_SIREN
void ListEntitiesSirenResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesSirenResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  for (const auto &it : this->tones) {
    out.append("  tones: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }

  out.append("  supports_duration: ");
  out.append(YESNO(this->supports_duration));
  out.append("\n");

  out.append("  supports_volume: ");
  out.append(YESNO(this->supports_volume));
  out.append("\n");

  out.append("  entity_category: ");
  out.append(proto_enum_to_string<enums::EntityCategory>(this->entity_category));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void SirenStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SirenStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void SirenCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SirenCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  has_state: ");
  out.append(YESNO(this->has_state));
  out.append("\n");

  out.append("  state: ");
  out.append(YESNO(this->state));
  out.append("\n");

  out.append("  has_tone: ");
  out.append(YESNO(this->has_tone));
  out.append("\n");

  out.append("  tone: ");
  out.append("'").append(this->tone).append("'");
  out.append("\n");

  out.append("  has_duration: ");
  out.append(YESNO(this->has_duration));
  out.append("\n");

  out.append("  duration: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->duration);
  out.append(buffer);
  out.append("\n");

  out.append("  has_volume: ");
  out.append(YESNO(this->has_volume));
  out.append("\n");

  out.append("  volume: ");
  snprintf(buffer, sizeof(buffer), "%g", this->volume);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_LOCK
void ListEntitiesLockResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesLockResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  assumed_state: ");
  out.append(YESNO(this->assumed_state));
  out.append("\n");

  out.append("  supports_open: ");
  out.append(YESNO(this->supports_open));
  out.append("\n");

  out.append("  requires_code: ");
  out.append(YESNO(this->requires_code));
  out.append("\n");

  out.append("  code_format: ");
  out.append("'").append(this->code_format).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void LockStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("LockStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(proto_enum_to_string<enums::LockState>(this->state));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void LockCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("LockCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  command: ");
  out.append(proto_enum_to_string<enums::LockCommand>(this->command));
  out.append("\n");

  out.append("  has_code: ");
  out.append(YESNO(this->has_code));
  out.append("\n");

  out.append("  code: ");
  out.append("'").append(this->code).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_BUTTON
void ListEntitiesButtonResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesButtonResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void ButtonCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ButtonCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_MEDIA_PLAYER
void MediaPlayerSupportedFormat::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("MediaPlayerSupportedFormat {\n");
  out.append("  format: ");
  out.append("'").append(this->format).append("'");
  out.append("\n");

  out.append("  sample_rate: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->sample_rate);
  out.append(buffer);
  out.append("\n");

  out.append("  num_channels: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->num_channels);
  out.append(buffer);
  out.append("\n");

  out.append("  purpose: ");
  out.append(proto_enum_to_string<enums::MediaPlayerFormatPurpose>(this->purpose));
  out.append("\n");

  out.append("  sample_bytes: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->sample_bytes);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void ListEntitiesMediaPlayerResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesMediaPlayerResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  supports_pause: ");
  out.append(YESNO(this->supports_pause));
  out.append("\n");

  for (const auto &it : this->supported_formats) {
    out.append("  supported_formats: ");
    it.dump_to(out);
    out.append("\n");
  }

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void MediaPlayerStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("MediaPlayerStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(proto_enum_to_string<enums::MediaPlayerState>(this->state));
  out.append("\n");

  out.append("  volume: ");
  snprintf(buffer, sizeof(buffer), "%g", this->volume);
  out.append(buffer);
  out.append("\n");

  out.append("  muted: ");
  out.append(YESNO(this->muted));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void MediaPlayerCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("MediaPlayerCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  has_command: ");
  out.append(YESNO(this->has_command));
  out.append("\n");

  out.append("  command: ");
  out.append(proto_enum_to_string<enums::MediaPlayerCommand>(this->command));
  out.append("\n");

  out.append("  has_volume: ");
  out.append(YESNO(this->has_volume));
  out.append("\n");

  out.append("  volume: ");
  snprintf(buffer, sizeof(buffer), "%g", this->volume);
  out.append(buffer);
  out.append("\n");

  out.append("  has_media_url: ");
  out.append(YESNO(this->has_media_url));
  out.append("\n");

  out.append("  media_url: ");
  out.append("'").append(this->media_url).append("'");
  out.append("\n");

  out.append("  has_announcement: ");
  out.append(YESNO(this->has_announcement));
  out.append("\n");

  out.append("  announcement: ");
  out.append(YESNO(this->announcement));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_BLUETOOTH_PROXY
void SubscribeBluetoothLEAdvertisementsRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SubscribeBluetoothLEAdvertisementsRequest {\n");
  out.append("  flags: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->flags);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothServiceData::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothServiceData {\n");
  out.append("  uuid: ");
  out.append("'").append(this->uuid).append("'");
  out.append("\n");

  for (const auto &it : this->legacy_data) {
    out.append("  legacy_data: ");
    snprintf(buffer, sizeof(buffer), "%" PRIu32, it);
    out.append(buffer);
    out.append("\n");
  }

  out.append("  data: ");
  out.append(format_hex_pretty(this->data));
  out.append("\n");
  out.append("}");
}
void BluetoothLEAdvertisementResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothLEAdvertisementResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  name: ");
  out.append(format_hex_pretty(this->name));
  out.append("\n");

  out.append("  rssi: ");
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->rssi);
  out.append(buffer);
  out.append("\n");

  for (const auto &it : this->service_uuids) {
    out.append("  service_uuids: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }

  for (const auto &it : this->service_data) {
    out.append("  service_data: ");
    it.dump_to(out);
    out.append("\n");
  }

  for (const auto &it : this->manufacturer_data) {
    out.append("  manufacturer_data: ");
    it.dump_to(out);
    out.append("\n");
  }

  out.append("  address_type: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->address_type);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothLERawAdvertisement::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothLERawAdvertisement {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  rssi: ");
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->rssi);
  out.append(buffer);
  out.append("\n");

  out.append("  address_type: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->address_type);
  out.append(buffer);
  out.append("\n");

  out.append("  data: ");
  out.append(format_hex_pretty(this->data));
  out.append("\n");
  out.append("}");
}
void BluetoothLERawAdvertisementsResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothLERawAdvertisementsResponse {\n");
  for (const auto &it : this->advertisements) {
    out.append("  advertisements: ");
    it.dump_to(out);
    out.append("\n");
  }
  out.append("}");
}
void BluetoothDeviceRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothDeviceRequest {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  request_type: ");
  out.append(proto_enum_to_string<enums::BluetoothDeviceRequestType>(this->request_type));
  out.append("\n");

  out.append("  has_address_type: ");
  out.append(YESNO(this->has_address_type));
  out.append("\n");

  out.append("  address_type: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->address_type);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothDeviceConnectionResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothDeviceConnectionResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  connected: ");
  out.append(YESNO(this->connected));
  out.append("\n");

  out.append("  mtu: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->mtu);
  out.append(buffer);
  out.append("\n");

  out.append("  error: ");
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->error);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothGATTGetServicesRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTGetServicesRequest {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothGATTDescriptor::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTDescriptor {\n");
  for (const auto &it : this->uuid) {
    out.append("  uuid: ");
    snprintf(buffer, sizeof(buffer), "%llu", it);
    out.append(buffer);
    out.append("\n");
  }

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothGATTCharacteristic::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTCharacteristic {\n");
  for (const auto &it : this->uuid) {
    out.append("  uuid: ");
    snprintf(buffer, sizeof(buffer), "%llu", it);
    out.append(buffer);
    out.append("\n");
  }

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");

  out.append("  properties: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->properties);
  out.append(buffer);
  out.append("\n");

  for (const auto &it : this->descriptors) {
    out.append("  descriptors: ");
    it.dump_to(out);
    out.append("\n");
  }
  out.append("}");
}
void BluetoothGATTService::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTService {\n");
  for (const auto &it : this->uuid) {
    out.append("  uuid: ");
    snprintf(buffer, sizeof(buffer), "%llu", it);
    out.append(buffer);
    out.append("\n");
  }

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");

  for (const auto &it : this->characteristics) {
    out.append("  characteristics: ");
    it.dump_to(out);
    out.append("\n");
  }
  out.append("}");
}
void BluetoothGATTGetServicesResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTGetServicesResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  for (const auto &it : this->services) {
    out.append("  services: ");
    it.dump_to(out);
    out.append("\n");
  }
  out.append("}");
}
void BluetoothGATTGetServicesDoneResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTGetServicesDoneResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothGATTReadRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTReadRequest {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothGATTReadResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTReadResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");

  out.append("  data: ");
  out.append(format_hex_pretty(this->data));
  out.append("\n");
  out.append("}");
}
void BluetoothGATTWriteRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTWriteRequest {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");

  out.append("  response: ");
  out.append(YESNO(this->response));
  out.append("\n");

  out.append("  data: ");
  out.append(format_hex_pretty(this->data));
  out.append("\n");
  out.append("}");
}
void BluetoothGATTReadDescriptorRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTReadDescriptorRequest {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothGATTWriteDescriptorRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTWriteDescriptorRequest {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");

  out.append("  data: ");
  out.append(format_hex_pretty(this->data));
  out.append("\n");
  out.append("}");
}
void BluetoothGATTNotifyRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTNotifyRequest {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");

  out.append("  enable: ");
  out.append(YESNO(this->enable));
  out.append("\n");
  out.append("}");
}
void BluetoothGATTNotifyDataResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTNotifyDataResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");

  out.append("  data: ");
  out.append(format_hex_pretty(this->data));
  out.append("\n");
  out.append("}");
}
void SubscribeBluetoothConnectionsFreeRequest::dump_to(std::string &out) const {
  out.append("SubscribeBluetoothConnectionsFreeRequest {}");
}
void BluetoothConnectionsFreeResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothConnectionsFreeResponse {\n");
  out.append("  free: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->free);
  out.append(buffer);
  out.append("\n");

  out.append("  limit: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->limit);
  out.append(buffer);
  out.append("\n");

  for (const auto &it : this->allocated) {
    out.append("  allocated: ");
    snprintf(buffer, sizeof(buffer), "%llu", it);
    out.append(buffer);
    out.append("\n");
  }
  out.append("}");
}
void BluetoothGATTErrorResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTErrorResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");

  out.append("  error: ");
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->error);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothGATTWriteResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTWriteResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothGATTNotifyResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothGATTNotifyResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  handle: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->handle);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothDevicePairingResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothDevicePairingResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  paired: ");
  out.append(YESNO(this->paired));
  out.append("\n");

  out.append("  error: ");
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->error);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothDeviceUnpairingResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothDeviceUnpairingResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  success: ");
  out.append(YESNO(this->success));
  out.append("\n");

  out.append("  error: ");
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->error);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void UnsubscribeBluetoothLEAdvertisementsRequest::dump_to(std::string &out) const {
  out.append("UnsubscribeBluetoothLEAdvertisementsRequest {}");
}
void BluetoothDeviceClearCacheResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothDeviceClearCacheResponse {\n");
  out.append("  address: ");
  snprintf(buffer, sizeof(buffer), "%llu", this->address);
  out.append(buffer);
  out.append("\n");

  out.append("  success: ");
  out.append(YESNO(this->success));
  out.append("\n");

  out.append("  error: ");
  snprintf(buffer, sizeof(buffer), "%" PRId32, this->error);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void BluetoothScannerStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothScannerStateResponse {\n");
  out.append("  state: ");
  out.append(proto_enum_to_string<enums::BluetoothScannerState>(this->state));
  out.append("\n");

  out.append("  mode: ");
  out.append(proto_enum_to_string<enums::BluetoothScannerMode>(this->mode));
  out.append("\n");
  out.append("}");
}
void BluetoothScannerSetModeRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("BluetoothScannerSetModeRequest {\n");
  out.append("  mode: ");
  out.append(proto_enum_to_string<enums::BluetoothScannerMode>(this->mode));
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_VOICE_ASSISTANT
void SubscribeVoiceAssistantRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("SubscribeVoiceAssistantRequest {\n");
  out.append("  subscribe: ");
  out.append(YESNO(this->subscribe));
  out.append("\n");

  out.append("  flags: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->flags);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void VoiceAssistantAudioSettings::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantAudioSettings {\n");
  out.append("  noise_suppression_level: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->noise_suppression_level);
  out.append(buffer);
  out.append("\n");

  out.append("  auto_gain: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->auto_gain);
  out.append(buffer);
  out.append("\n");

  out.append("  volume_multiplier: ");
  snprintf(buffer, sizeof(buffer), "%g", this->volume_multiplier);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void VoiceAssistantRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantRequest {\n");
  out.append("  start: ");
  out.append(YESNO(this->start));
  out.append("\n");

  out.append("  conversation_id: ");
  out.append("'").append(this->conversation_id).append("'");
  out.append("\n");

  out.append("  flags: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->flags);
  out.append(buffer);
  out.append("\n");

  out.append("  audio_settings: ");
  this->audio_settings.dump_to(out);
  out.append("\n");

  out.append("  wake_word_phrase: ");
  out.append("'").append(this->wake_word_phrase).append("'");
  out.append("\n");
  out.append("}");
}
void VoiceAssistantResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantResponse {\n");
  out.append("  port: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->port);
  out.append(buffer);
  out.append("\n");

  out.append("  error: ");
  out.append(YESNO(this->error));
  out.append("\n");
  out.append("}");
}
void VoiceAssistantEventData::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantEventData {\n");
  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  value: ");
  out.append("'").append(this->value).append("'");
  out.append("\n");
  out.append("}");
}
void VoiceAssistantEventResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantEventResponse {\n");
  out.append("  event_type: ");
  out.append(proto_enum_to_string<enums::VoiceAssistantEvent>(this->event_type));
  out.append("\n");

  for (const auto &it : this->data) {
    out.append("  data: ");
    it.dump_to(out);
    out.append("\n");
  }
  out.append("}");
}
void VoiceAssistantAudio::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantAudio {\n");
  out.append("  data: ");
  out.append(format_hex_pretty(this->data));
  out.append("\n");

  out.append("  end: ");
  out.append(YESNO(this->end));
  out.append("\n");
  out.append("}");
}
void VoiceAssistantTimerEventResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantTimerEventResponse {\n");
  out.append("  event_type: ");
  out.append(proto_enum_to_string<enums::VoiceAssistantTimerEvent>(this->event_type));
  out.append("\n");

  out.append("  timer_id: ");
  out.append("'").append(this->timer_id).append("'");
  out.append("\n");

  out.append("  name: ");
  out.append("'").append(this->name).append("'");
  out.append("\n");

  out.append("  total_seconds: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->total_seconds);
  out.append(buffer);
  out.append("\n");

  out.append("  seconds_left: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->seconds_left);
  out.append(buffer);
  out.append("\n");

  out.append("  is_active: ");
  out.append(YESNO(this->is_active));
  out.append("\n");
  out.append("}");
}
void VoiceAssistantAnnounceRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantAnnounceRequest {\n");
  out.append("  media_id: ");
  out.append("'").append(this->media_id).append("'");
  out.append("\n");

  out.append("  text: ");
  out.append("'").append(this->text).append("'");
  out.append("\n");

  out.append("  preannounce_media_id: ");
  out.append("'").append(this->preannounce_media_id).append("'");
  out.append("\n");

  out.append("  start_conversation: ");
  out.append(YESNO(this->start_conversation));
  out.append("\n");
  out.append("}");
}
void VoiceAssistantAnnounceFinished::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantAnnounceFinished {\n");
  out.append("  success: ");
  out.append(YESNO(this->success));
  out.append("\n");
  out.append("}");
}
void VoiceAssistantWakeWord::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantWakeWord {\n");
  out.append("  id: ");
  out.append("'").append(this->id).append("'");
  out.append("\n");

  out.append("  wake_word: ");
  out.append("'").append(this->wake_word).append("'");
  out.append("\n");

  for (const auto &it : this->trained_languages) {
    out.append("  trained_languages: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }
  out.append("}");
}
void VoiceAssistantConfigurationRequest::dump_to(std::string &out) const {
  out.append("VoiceAssistantConfigurationRequest {}");
}
void VoiceAssistantConfigurationResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantConfigurationResponse {\n");
  for (const auto &it : this->available_wake_words) {
    out.append("  available_wake_words: ");
    it.dump_to(out);
    out.append("\n");
  }

  for (const auto &it : this->active_wake_words) {
    out.append("  active_wake_words: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }

  out.append("  max_active_wake_words: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->max_active_wake_words);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void VoiceAssistantSetConfiguration::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("VoiceAssistantSetConfiguration {\n");
  for (const auto &it : this->active_wake_words) {
    out.append("  active_wake_words: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }
  out.append("}");
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
void ListEntitiesAlarmControlPanelResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesAlarmControlPanelResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  supported_features: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->supported_features);
  out.append(buffer);
  out.append("\n");

  out.append("  requires_code: ");
  out.append(YESNO(this->requires_code));
  out.append("\n");

  out.append("  requires_code_to_arm: ");
  out.append(YESNO(this->requires_code_to_arm));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void AlarmControlPanelStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("AlarmControlPanelStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append(proto_enum_to_string<enums::AlarmControlPanelState>(this->state));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void AlarmControlPanelCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("AlarmControlPanelCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  command: ");
  out.append(proto_enum_to_string<enums::AlarmControlPanelStateCommand>(this->command));
  out.append("\n");

  out.append("  code: ");
  out.append("'").append(this->code).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_TEXT
void ListEntitiesTextResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesTextResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  min_length: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->min_length);
  out.append(buffer);
  out.append("\n");

  out.append("  max_length: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->max_length);
  out.append(buffer);
  out.append("\n");

  out.append("  pattern: ");
  out.append("'").append(this->pattern).append("'");
  out.append("\n");

  out.append("  mode: ");
  out.append(proto_enum_to_string<enums::TextMode>(this->mode));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void TextStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("TextStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append("'").append(this->state).append("'");
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void TextCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("TextCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  state: ");
  out.append("'").append(this->state).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_DATETIME_DATE
void ListEntitiesDateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesDateResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void DateStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("DateStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");

  out.append("  year: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->year);
  out.append(buffer);
  out.append("\n");

  out.append("  month: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->month);
  out.append(buffer);
  out.append("\n");

  out.append("  day: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->day);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void DateCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("DateCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  year: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->year);
  out.append(buffer);
  out.append("\n");

  out.append("  month: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->month);
  out.append(buffer);
  out.append("\n");

  out.append("  day: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->day);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_DATETIME_TIME
void ListEntitiesTimeResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesTimeResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void TimeStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("TimeStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");

  out.append("  hour: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->hour);
  out.append(buffer);
  out.append("\n");

  out.append("  minute: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->minute);
  out.append(buffer);
  out.append("\n");

  out.append("  second: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->second);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void TimeCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("TimeCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  hour: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->hour);
  out.append(buffer);
  out.append("\n");

  out.append("  minute: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->minute);
  out.append(buffer);
  out.append("\n");

  out.append("  second: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->second);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_EVENT
void ListEntitiesEventResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesEventResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  for (const auto &it : this->event_types) {
    out.append("  event_types: ");
    out.append("'").append(it).append("'");
    out.append("\n");
  }

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void EventResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("EventResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  event_type: ");
  out.append("'").append(this->event_type).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_VALVE
void ListEntitiesValveResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesValveResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  assumed_state: ");
  out.append(YESNO(this->assumed_state));
  out.append("\n");

  out.append("  supports_position: ");
  out.append(YESNO(this->supports_position));
  out.append("\n");

  out.append("  supports_stop: ");
  out.append(YESNO(this->supports_stop));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void ValveStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ValveStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  position: ");
  snprintf(buffer, sizeof(buffer), "%g", this->position);
  out.append(buffer);
  out.append("\n");

  out.append("  current_operation: ");
  out.append(proto_enum_to_string<enums::ValveOperation>(this->current_operation));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void ValveCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ValveCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  has_position: ");
  out.append(YESNO(this->has_position));
  out.append("\n");

  out.append("  position: ");
  snprintf(buffer, sizeof(buffer), "%g", this->position);
  out.append(buffer);
  out.append("\n");

  out.append("  stop: ");
  out.append(YESNO(this->stop));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_DATETIME_DATETIME
void ListEntitiesDateTimeResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesDateTimeResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void DateTimeStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("DateTimeStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");

  out.append("  epoch_seconds: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->epoch_seconds);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void DateTimeCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("DateTimeCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  epoch_seconds: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->epoch_seconds);
  out.append(buffer);
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif
#ifdef USE_UPDATE
void ListEntitiesUpdateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("ListEntitiesUpdateResponse {\n");
  out.append("  object_id: ");
  out.append("'").append(this->object_id).append("'");
  out.append("\n");

  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
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

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void UpdateStateResponse::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("UpdateStateResponse {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  missing_state: ");
  out.append(YESNO(this->missing_state));
  out.append("\n");

  out.append("  in_progress: ");
  out.append(YESNO(this->in_progress));
  out.append("\n");

  out.append("  has_progress: ");
  out.append(YESNO(this->has_progress));
  out.append("\n");

  out.append("  progress: ");
  snprintf(buffer, sizeof(buffer), "%g", this->progress);
  out.append(buffer);
  out.append("\n");

  out.append("  current_version: ");
  out.append("'").append(this->current_version).append("'");
  out.append("\n");

  out.append("  latest_version: ");
  out.append("'").append(this->latest_version).append("'");
  out.append("\n");

  out.append("  title: ");
  out.append("'").append(this->title).append("'");
  out.append("\n");

  out.append("  release_summary: ");
  out.append("'").append(this->release_summary).append("'");
  out.append("\n");

  out.append("  release_url: ");
  out.append("'").append(this->release_url).append("'");
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
void UpdateCommandRequest::dump_to(std::string &out) const {
  __attribute__((unused)) char buffer[64];
  out.append("UpdateCommandRequest {\n");
  out.append("  key: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->key);
  out.append(buffer);
  out.append("\n");

  out.append("  command: ");
  out.append(proto_enum_to_string<enums::UpdateCommand>(this->command));
  out.append("\n");

  out.append("  device_id: ");
  snprintf(buffer, sizeof(buffer), "%" PRIu32, this->device_id);
  out.append(buffer);
  out.append("\n");
  out.append("}");
}
#endif

}  // namespace api
}  // namespace esphome

#endif  // HAS_PROTO_MESSAGE_DUMP
