// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#include "api_pb2.h"
#include "esphome/core/helpers.h"

#include <cinttypes>

#ifdef HAS_PROTO_MESSAGE_DUMP

namespace esphome::api {

// Helper function to append a quoted string, handling empty StringRef
static inline void append_quoted_string(DumpBuffer &out, const StringRef &ref) {
  out.append("'");
  if (!ref.empty()) {
    out.append(ref.c_str());
  }
  out.append("'");
}

// Common helpers for dump_field functions
static inline void append_field_prefix(DumpBuffer &out, const char *field_name, int indent) {
  out.append(indent, ' ').append(field_name).append(": ");
}

static inline void append_with_newline(DumpBuffer &out, const char *str) {
  out.append(str);
  out.append("\n");
}

static inline void append_uint(DumpBuffer &out, uint32_t value) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%" PRIu32, value);
  out.append(buf);
}

// RAII helper for message dump formatting
class MessageDumpHelper {
 public:
  MessageDumpHelper(DumpBuffer &out, const char *message_name) : out_(out) {
    out_.append(message_name);
    out_.append(" {\n");
  }
  ~MessageDumpHelper() { out_.append(" }"); }

 private:
  DumpBuffer &out_;
};

// Helper functions to reduce code duplication in dump methods
static void dump_field(DumpBuffer &out, const char *field_name, int32_t value, int indent = 2) {
  char buffer[64];
  append_field_prefix(out, field_name, indent);
  snprintf(buffer, 64, "%" PRId32, value);
  append_with_newline(out, buffer);
}

static void dump_field(DumpBuffer &out, const char *field_name, uint32_t value, int indent = 2) {
  char buffer[64];
  append_field_prefix(out, field_name, indent);
  snprintf(buffer, 64, "%" PRIu32, value);
  append_with_newline(out, buffer);
}

static void dump_field(DumpBuffer &out, const char *field_name, float value, int indent = 2) {
  char buffer[64];
  append_field_prefix(out, field_name, indent);
  snprintf(buffer, 64, "%g", value);
  append_with_newline(out, buffer);
}

static void dump_field(DumpBuffer &out, const char *field_name, uint64_t value, int indent = 2) {
  char buffer[64];
  append_field_prefix(out, field_name, indent);
  snprintf(buffer, 64, "%" PRIu64, value);
  append_with_newline(out, buffer);
}

static void dump_field(DumpBuffer &out, const char *field_name, bool value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.append(YESNO(value));
  out.append("\n");
}

static void dump_field(DumpBuffer &out, const char *field_name, const std::string &value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.append("'").append(value.c_str()).append("'");
  out.append("\n");
}

static void dump_field(DumpBuffer &out, const char *field_name, StringRef value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  append_quoted_string(out, value);
  out.append("\n");
}

static void dump_field(DumpBuffer &out, const char *field_name, const char *value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.append("'").append(value).append("'");
  out.append("\n");
}

template<typename T> static void dump_field(DumpBuffer &out, const char *field_name, T value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.append(proto_enum_to_string<T>(value));
  out.append("\n");
}

// Helper for bytes fields - uses stack buffer to avoid heap allocation
// Buffer sized for 160 bytes of data (480 chars with separators) to fit typical log buffer
static void dump_bytes_field(DumpBuffer &out, const char *field_name, const uint8_t *data, size_t len, int indent = 2) {
  char hex_buf[format_hex_pretty_size(160)];
  append_field_prefix(out, field_name, indent);
  format_hex_pretty_to(hex_buf, data, len);
  append_with_newline(out, hex_buf);
}

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
#endif
#ifdef USE_FAN
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
    case enums::STATE_CLASS_MEASUREMENT_ANGLE:
      return "STATE_CLASS_MEASUREMENT_ANGLE";
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
#ifdef USE_API_USER_DEFINED_ACTIONS
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
template<> const char *proto_enum_to_string<enums::SupportsResponseType>(enums::SupportsResponseType value) {
  switch (value) {
    case enums::SUPPORTS_RESPONSE_NONE:
      return "SUPPORTS_RESPONSE_NONE";
    case enums::SUPPORTS_RESPONSE_OPTIONAL:
      return "SUPPORTS_RESPONSE_OPTIONAL";
    case enums::SUPPORTS_RESPONSE_ONLY:
      return "SUPPORTS_RESPONSE_ONLY";
    case enums::SUPPORTS_RESPONSE_STATUS:
      return "SUPPORTS_RESPONSE_STATUS";
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
#ifdef USE_WATER_HEATER
template<> const char *proto_enum_to_string<enums::WaterHeaterMode>(enums::WaterHeaterMode value) {
  switch (value) {
    case enums::WATER_HEATER_MODE_OFF:
      return "WATER_HEATER_MODE_OFF";
    case enums::WATER_HEATER_MODE_ECO:
      return "WATER_HEATER_MODE_ECO";
    case enums::WATER_HEATER_MODE_ELECTRIC:
      return "WATER_HEATER_MODE_ELECTRIC";
    case enums::WATER_HEATER_MODE_PERFORMANCE:
      return "WATER_HEATER_MODE_PERFORMANCE";
    case enums::WATER_HEATER_MODE_HIGH_DEMAND:
      return "WATER_HEATER_MODE_HIGH_DEMAND";
    case enums::WATER_HEATER_MODE_HEAT_PUMP:
      return "WATER_HEATER_MODE_HEAT_PUMP";
    case enums::WATER_HEATER_MODE_GAS:
      return "WATER_HEATER_MODE_GAS";
    default:
      return "UNKNOWN";
  }
}
#endif
template<>
const char *proto_enum_to_string<enums::WaterHeaterCommandHasField>(enums::WaterHeaterCommandHasField value) {
  switch (value) {
    case enums::WATER_HEATER_COMMAND_HAS_NONE:
      return "WATER_HEATER_COMMAND_HAS_NONE";
    case enums::WATER_HEATER_COMMAND_HAS_MODE:
      return "WATER_HEATER_COMMAND_HAS_MODE";
    case enums::WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE:
      return "WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE";
    case enums::WATER_HEATER_COMMAND_HAS_STATE:
      return "WATER_HEATER_COMMAND_HAS_STATE";
    case enums::WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE_LOW:
      return "WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE_LOW";
    case enums::WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE_HIGH:
      return "WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE_HIGH";
    default:
      return "UNKNOWN";
  }
}
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
    case enums::MEDIA_PLAYER_STATE_ANNOUNCING:
      return "MEDIA_PLAYER_STATE_ANNOUNCING";
    case enums::MEDIA_PLAYER_STATE_OFF:
      return "MEDIA_PLAYER_STATE_OFF";
    case enums::MEDIA_PLAYER_STATE_ON:
      return "MEDIA_PLAYER_STATE_ON";
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
    case enums::MEDIA_PLAYER_COMMAND_TOGGLE:
      return "MEDIA_PLAYER_COMMAND_TOGGLE";
    case enums::MEDIA_PLAYER_COMMAND_VOLUME_UP:
      return "MEDIA_PLAYER_COMMAND_VOLUME_UP";
    case enums::MEDIA_PLAYER_COMMAND_VOLUME_DOWN:
      return "MEDIA_PLAYER_COMMAND_VOLUME_DOWN";
    case enums::MEDIA_PLAYER_COMMAND_ENQUEUE:
      return "MEDIA_PLAYER_COMMAND_ENQUEUE";
    case enums::MEDIA_PLAYER_COMMAND_REPEAT_ONE:
      return "MEDIA_PLAYER_COMMAND_REPEAT_ONE";
    case enums::MEDIA_PLAYER_COMMAND_REPEAT_OFF:
      return "MEDIA_PLAYER_COMMAND_REPEAT_OFF";
    case enums::MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST:
      return "MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST";
    case enums::MEDIA_PLAYER_COMMAND_TURN_ON:
      return "MEDIA_PLAYER_COMMAND_TURN_ON";
    case enums::MEDIA_PLAYER_COMMAND_TURN_OFF:
      return "MEDIA_PLAYER_COMMAND_TURN_OFF";
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
#ifdef USE_ZWAVE_PROXY
template<> const char *proto_enum_to_string<enums::ZWaveProxyRequestType>(enums::ZWaveProxyRequestType value) {
  switch (value) {
    case enums::ZWAVE_PROXY_REQUEST_TYPE_SUBSCRIBE:
      return "ZWAVE_PROXY_REQUEST_TYPE_SUBSCRIBE";
    case enums::ZWAVE_PROXY_REQUEST_TYPE_UNSUBSCRIBE:
      return "ZWAVE_PROXY_REQUEST_TYPE_UNSUBSCRIBE";
    case enums::ZWAVE_PROXY_REQUEST_TYPE_HOME_ID_CHANGE:
      return "ZWAVE_PROXY_REQUEST_TYPE_HOME_ID_CHANGE";
    default:
      return "UNKNOWN";
  }
}
#endif

const char *HelloRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "HelloRequest");
  dump_field(out, "client_info", this->client_info);
  dump_field(out, "api_version_major", this->api_version_major);
  dump_field(out, "api_version_minor", this->api_version_minor);
  return out.c_str();
}
const char *HelloResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "HelloResponse");
  dump_field(out, "api_version_major", this->api_version_major);
  dump_field(out, "api_version_minor", this->api_version_minor);
  dump_field(out, "server_info", this->server_info);
  dump_field(out, "name", this->name);
  return out.c_str();
}
const char *DisconnectRequest::dump_to(DumpBuffer &out) const {
  out.append("DisconnectRequest {}");
  return out.c_str();
}
const char *DisconnectResponse::dump_to(DumpBuffer &out) const {
  out.append("DisconnectResponse {}");
  return out.c_str();
}
const char *PingRequest::dump_to(DumpBuffer &out) const {
  out.append("PingRequest {}");
  return out.c_str();
}
const char *PingResponse::dump_to(DumpBuffer &out) const {
  out.append("PingResponse {}");
  return out.c_str();
}
const char *DeviceInfoRequest::dump_to(DumpBuffer &out) const {
  out.append("DeviceInfoRequest {}");
  return out.c_str();
}
#ifdef USE_AREAS
const char *AreaInfo::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "AreaInfo");
  dump_field(out, "area_id", this->area_id);
  dump_field(out, "name", this->name);
  return out.c_str();
}
#endif
#ifdef USE_DEVICES
const char *DeviceInfo::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "DeviceInfo");
  dump_field(out, "device_id", this->device_id);
  dump_field(out, "name", this->name);
  dump_field(out, "area_id", this->area_id);
  return out.c_str();
}
#endif
const char *DeviceInfoResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "DeviceInfoResponse");
  dump_field(out, "name", this->name);
  dump_field(out, "mac_address", this->mac_address);
  dump_field(out, "esphome_version", this->esphome_version);
  dump_field(out, "compilation_time", this->compilation_time);
  dump_field(out, "model", this->model);
#ifdef USE_DEEP_SLEEP
  dump_field(out, "has_deep_sleep", this->has_deep_sleep);
#endif
#ifdef ESPHOME_PROJECT_NAME
  dump_field(out, "project_name", this->project_name);
#endif
#ifdef ESPHOME_PROJECT_NAME
  dump_field(out, "project_version", this->project_version);
#endif
#ifdef USE_WEBSERVER
  dump_field(out, "webserver_port", this->webserver_port);
#endif
#ifdef USE_BLUETOOTH_PROXY
  dump_field(out, "bluetooth_proxy_feature_flags", this->bluetooth_proxy_feature_flags);
#endif
  dump_field(out, "manufacturer", this->manufacturer);
  dump_field(out, "friendly_name", this->friendly_name);
#ifdef USE_VOICE_ASSISTANT
  dump_field(out, "voice_assistant_feature_flags", this->voice_assistant_feature_flags);
#endif
#ifdef USE_AREAS
  dump_field(out, "suggested_area", this->suggested_area);
#endif
#ifdef USE_BLUETOOTH_PROXY
  dump_field(out, "bluetooth_mac_address", this->bluetooth_mac_address);
#endif
#ifdef USE_API_NOISE
  dump_field(out, "api_encryption_supported", this->api_encryption_supported);
#endif
#ifdef USE_DEVICES
  for (const auto &it : this->devices) {
    out.append("  devices: ");
    it.dump_to(out);
    out.append("\n");
  }
#endif
#ifdef USE_AREAS
  for (const auto &it : this->areas) {
    out.append("  areas: ");
    it.dump_to(out);
    out.append("\n");
  }
#endif
#ifdef USE_AREAS
  out.append("  area: ");
  this->area.dump_to(out);
  out.append("\n");
#endif
#ifdef USE_ZWAVE_PROXY
  dump_field(out, "zwave_proxy_feature_flags", this->zwave_proxy_feature_flags);
#endif
#ifdef USE_ZWAVE_PROXY
  dump_field(out, "zwave_home_id", this->zwave_home_id);
#endif
  return out.c_str();
}
const char *ListEntitiesRequest::dump_to(DumpBuffer &out) const {
  out.append("ListEntitiesRequest {}");
  return out.c_str();
}
const char *ListEntitiesDoneResponse::dump_to(DumpBuffer &out) const {
  out.append("ListEntitiesDoneResponse {}");
  return out.c_str();
}
const char *SubscribeStatesRequest::dump_to(DumpBuffer &out) const {
  out.append("SubscribeStatesRequest {}");
  return out.c_str();
}
#ifdef USE_BINARY_SENSOR
const char *ListEntitiesBinarySensorResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesBinarySensorResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
  dump_field(out, "device_class", this->device_class);
  dump_field(out, "is_status_binary_sensor", this->is_status_binary_sensor);
  dump_field(out, "disabled_by_default", this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *BinarySensorStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BinarySensorStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
  dump_field(out, "missing_state", this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_COVER
const char *ListEntitiesCoverResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesCoverResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
  dump_field(out, "assumed_state", this->assumed_state);
  dump_field(out, "supports_position", this->supports_position);
  dump_field(out, "supports_tilt", this->supports_tilt);
  dump_field(out, "device_class", this->device_class);
  dump_field(out, "disabled_by_default", this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "supports_stop", this->supports_stop);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *CoverStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "CoverStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "position", this->position);
  dump_field(out, "tilt", this->tilt);
  dump_field(out, "current_operation", static_cast<enums::CoverOperation>(this->current_operation));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *CoverCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "CoverCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "has_position", this->has_position);
  dump_field(out, "position", this->position);
  dump_field(out, "has_tilt", this->has_tilt);
  dump_field(out, "tilt", this->tilt);
  dump_field(out, "stop", this->stop);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_FAN
const char *ListEntitiesFanResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesFanResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
  dump_field(out, "supports_oscillation", this->supports_oscillation);
  dump_field(out, "supports_speed", this->supports_speed);
  dump_field(out, "supports_direction", this->supports_direction);
  dump_field(out, "supported_speed_count", this->supported_speed_count);
  dump_field(out, "disabled_by_default", this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  for (const auto &it : *this->supported_preset_modes) {
    dump_field(out, "supported_preset_modes", it, 4);
  }
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *FanStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "FanStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
  dump_field(out, "oscillating", this->oscillating);
  dump_field(out, "direction", static_cast<enums::FanDirection>(this->direction));
  dump_field(out, "speed_level", this->speed_level);
  dump_field(out, "preset_mode", this->preset_mode);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *FanCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "FanCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "has_state", this->has_state);
  dump_field(out, "state", this->state);
  dump_field(out, "has_oscillating", this->has_oscillating);
  dump_field(out, "oscillating", this->oscillating);
  dump_field(out, "has_direction", this->has_direction);
  dump_field(out, "direction", static_cast<enums::FanDirection>(this->direction));
  dump_field(out, "has_speed_level", this->has_speed_level);
  dump_field(out, "speed_level", this->speed_level);
  dump_field(out, "has_preset_mode", this->has_preset_mode);
  dump_field(out, "preset_mode", this->preset_mode);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_LIGHT
const char *ListEntitiesLightResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesLightResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
  for (const auto &it : *this->supported_color_modes) {
    dump_field(out, "supported_color_modes", static_cast<enums::ColorMode>(it), 4);
  }
  dump_field(out, "min_mireds", this->min_mireds);
  dump_field(out, "max_mireds", this->max_mireds);
  for (const auto &it : *this->effects) {
    dump_field(out, "effects", it, 4);
  }
  dump_field(out, "disabled_by_default", this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *LightStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "LightStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
  dump_field(out, "brightness", this->brightness);
  dump_field(out, "color_mode", static_cast<enums::ColorMode>(this->color_mode));
  dump_field(out, "color_brightness", this->color_brightness);
  dump_field(out, "red", this->red);
  dump_field(out, "green", this->green);
  dump_field(out, "blue", this->blue);
  dump_field(out, "white", this->white);
  dump_field(out, "color_temperature", this->color_temperature);
  dump_field(out, "cold_white", this->cold_white);
  dump_field(out, "warm_white", this->warm_white);
  dump_field(out, "effect", this->effect);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *LightCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "LightCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "has_state", this->has_state);
  dump_field(out, "state", this->state);
  dump_field(out, "has_brightness", this->has_brightness);
  dump_field(out, "brightness", this->brightness);
  dump_field(out, "has_color_mode", this->has_color_mode);
  dump_field(out, "color_mode", static_cast<enums::ColorMode>(this->color_mode));
  dump_field(out, "has_color_brightness", this->has_color_brightness);
  dump_field(out, "color_brightness", this->color_brightness);
  dump_field(out, "has_rgb", this->has_rgb);
  dump_field(out, "red", this->red);
  dump_field(out, "green", this->green);
  dump_field(out, "blue", this->blue);
  dump_field(out, "has_white", this->has_white);
  dump_field(out, "white", this->white);
  dump_field(out, "has_color_temperature", this->has_color_temperature);
  dump_field(out, "color_temperature", this->color_temperature);
  dump_field(out, "has_cold_white", this->has_cold_white);
  dump_field(out, "cold_white", this->cold_white);
  dump_field(out, "has_warm_white", this->has_warm_white);
  dump_field(out, "warm_white", this->warm_white);
  dump_field(out, "has_transition_length", this->has_transition_length);
  dump_field(out, "transition_length", this->transition_length);
  dump_field(out, "has_flash_length", this->has_flash_length);
  dump_field(out, "flash_length", this->flash_length);
  dump_field(out, "has_effect", this->has_effect);
  dump_field(out, "effect", this->effect);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_SENSOR
const char *ListEntitiesSensorResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesSensorResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "unit_of_measurement", this->unit_of_measurement);
  dump_field(out, "accuracy_decimals", this->accuracy_decimals);
  dump_field(out, "force_update", this->force_update);
  dump_field(out, "device_class", this->device_class);
  dump_field(out, "state_class", static_cast<enums::SensorStateClass>(this->state_class));
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *SensorStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SensorStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
  dump_field(out, "missing_state", this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_SWITCH
const char *ListEntitiesSwitchResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesSwitchResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "assumed_state", this->assumed_state);
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "device_class", this->device_class);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *SwitchStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SwitchStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *SwitchCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SwitchCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_TEXT_SENSOR
const char *ListEntitiesTextSensorResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesTextSensorResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "device_class", this->device_class);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *TextSensorStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "TextSensorStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
  dump_field(out, "missing_state", this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
const char *SubscribeLogsRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SubscribeLogsRequest");
  dump_field(out, "level", static_cast<enums::LogLevel>(this->level));
  dump_field(out, "dump_config", this->dump_config);
  return out.c_str();
}
const char *SubscribeLogsResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SubscribeLogsResponse");
  dump_field(out, "level", static_cast<enums::LogLevel>(this->level));
  dump_bytes_field(out, "message", this->message_ptr_, this->message_len_);
  return out.c_str();
}
#ifdef USE_API_NOISE
const char *NoiseEncryptionSetKeyRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "NoiseEncryptionSetKeyRequest");
  dump_bytes_field(out, "key", this->key, this->key_len);
  return out.c_str();
}
const char *NoiseEncryptionSetKeyResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "NoiseEncryptionSetKeyResponse");
  dump_field(out, "success", this->success);
  return out.c_str();
}
#endif
#ifdef USE_API_HOMEASSISTANT_SERVICES
const char *SubscribeHomeassistantServicesRequest::dump_to(DumpBuffer &out) const {
  out.append("SubscribeHomeassistantServicesRequest {}");
  return out.c_str();
}
const char *HomeassistantServiceMap::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "HomeassistantServiceMap");
  dump_field(out, "key", this->key);
  dump_field(out, "value", this->value);
  return out.c_str();
}
const char *HomeassistantActionRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "HomeassistantActionRequest");
  dump_field(out, "service", this->service);
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
  dump_field(out, "is_event", this->is_event);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  dump_field(out, "call_id", this->call_id);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  dump_field(out, "wants_response", this->wants_response);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  dump_field(out, "response_template", this->response_template);
#endif
  return out.c_str();
}
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
const char *HomeassistantActionResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "HomeassistantActionResponse");
  dump_field(out, "call_id", this->call_id);
  dump_field(out, "success", this->success);
  dump_field(out, "error_message", this->error_message);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  dump_bytes_field(out, "response_data", this->response_data, this->response_data_len);
#endif
  return out.c_str();
}
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
const char *SubscribeHomeAssistantStatesRequest::dump_to(DumpBuffer &out) const {
  out.append("SubscribeHomeAssistantStatesRequest {}");
  return out.c_str();
}
const char *SubscribeHomeAssistantStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SubscribeHomeAssistantStateResponse");
  dump_field(out, "entity_id", this->entity_id);
  dump_field(out, "attribute", this->attribute);
  dump_field(out, "once", this->once);
  return out.c_str();
}
const char *HomeAssistantStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "HomeAssistantStateResponse");
  dump_field(out, "entity_id", this->entity_id);
  dump_field(out, "state", this->state);
  dump_field(out, "attribute", this->attribute);
  return out.c_str();
}
#endif
const char *GetTimeRequest::dump_to(DumpBuffer &out) const {
  out.append("GetTimeRequest {}");
  return out.c_str();
}
const char *GetTimeResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "GetTimeResponse");
  dump_field(out, "epoch_seconds", this->epoch_seconds);
  dump_field(out, "timezone", this->timezone);
  return out.c_str();
}
#ifdef USE_API_USER_DEFINED_ACTIONS
const char *ListEntitiesServicesArgument::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesServicesArgument");
  dump_field(out, "name", this->name);
  dump_field(out, "type", static_cast<enums::ServiceArgType>(this->type));
  return out.c_str();
}
const char *ListEntitiesServicesResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesServicesResponse");
  dump_field(out, "name", this->name);
  dump_field(out, "key", this->key);
  for (const auto &it : this->args) {
    out.append("  args: ");
    it.dump_to(out);
    out.append("\n");
  }
  dump_field(out, "supports_response", static_cast<enums::SupportsResponseType>(this->supports_response));
  return out.c_str();
}
const char *ExecuteServiceArgument::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ExecuteServiceArgument");
  dump_field(out, "bool_", this->bool_);
  dump_field(out, "legacy_int", this->legacy_int);
  dump_field(out, "float_", this->float_);
  dump_field(out, "string_", this->string_);
  dump_field(out, "int_", this->int_);
  for (const auto it : this->bool_array) {
    dump_field(out, "bool_array", static_cast<bool>(it), 4);
  }
  for (const auto &it : this->int_array) {
    dump_field(out, "int_array", it, 4);
  }
  for (const auto &it : this->float_array) {
    dump_field(out, "float_array", it, 4);
  }
  for (const auto &it : this->string_array) {
    dump_field(out, "string_array", it, 4);
  }
  return out.c_str();
}
const char *ExecuteServiceRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ExecuteServiceRequest");
  dump_field(out, "key", this->key);
  for (const auto &it : this->args) {
    out.append("  args: ");
    it.dump_to(out);
    out.append("\n");
  }
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  dump_field(out, "call_id", this->call_id);
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  dump_field(out, "return_response", this->return_response);
#endif
  return out.c_str();
}
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
const char *ExecuteServiceResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ExecuteServiceResponse");
  dump_field(out, "call_id", this->call_id);
  dump_field(out, "success", this->success);
  dump_field(out, "error_message", this->error_message);
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  dump_bytes_field(out, "response_data", this->response_data, this->response_data_len);
#endif
  return out.c_str();
}
#endif
#ifdef USE_CAMERA
const char *ListEntitiesCameraResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesCameraResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
  dump_field(out, "disabled_by_default", this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *CameraImageResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "CameraImageResponse");
  dump_field(out, "key", this->key);
  dump_bytes_field(out, "data", this->data_ptr_, this->data_len_);
  dump_field(out, "done", this->done);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *CameraImageRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "CameraImageRequest");
  dump_field(out, "single", this->single);
  dump_field(out, "stream", this->stream);
  return out.c_str();
}
#endif
#ifdef USE_CLIMATE
const char *ListEntitiesClimateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesClimateResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
  dump_field(out, "supports_current_temperature", this->supports_current_temperature);
  dump_field(out, "supports_two_point_target_temperature", this->supports_two_point_target_temperature);
  for (const auto &it : *this->supported_modes) {
    dump_field(out, "supported_modes", static_cast<enums::ClimateMode>(it), 4);
  }
  dump_field(out, "visual_min_temperature", this->visual_min_temperature);
  dump_field(out, "visual_max_temperature", this->visual_max_temperature);
  dump_field(out, "visual_target_temperature_step", this->visual_target_temperature_step);
  dump_field(out, "supports_action", this->supports_action);
  for (const auto &it : *this->supported_fan_modes) {
    dump_field(out, "supported_fan_modes", static_cast<enums::ClimateFanMode>(it), 4);
  }
  for (const auto &it : *this->supported_swing_modes) {
    dump_field(out, "supported_swing_modes", static_cast<enums::ClimateSwingMode>(it), 4);
  }
  for (const auto &it : *this->supported_custom_fan_modes) {
    dump_field(out, "supported_custom_fan_modes", it, 4);
  }
  for (const auto &it : *this->supported_presets) {
    dump_field(out, "supported_presets", static_cast<enums::ClimatePreset>(it), 4);
  }
  for (const auto &it : *this->supported_custom_presets) {
    dump_field(out, "supported_custom_presets", it, 4);
  }
  dump_field(out, "disabled_by_default", this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "visual_current_temperature_step", this->visual_current_temperature_step);
  dump_field(out, "supports_current_humidity", this->supports_current_humidity);
  dump_field(out, "supports_target_humidity", this->supports_target_humidity);
  dump_field(out, "visual_min_humidity", this->visual_min_humidity);
  dump_field(out, "visual_max_humidity", this->visual_max_humidity);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  dump_field(out, "feature_flags", this->feature_flags);
  return out.c_str();
}
const char *ClimateStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ClimateStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "mode", static_cast<enums::ClimateMode>(this->mode));
  dump_field(out, "current_temperature", this->current_temperature);
  dump_field(out, "target_temperature", this->target_temperature);
  dump_field(out, "target_temperature_low", this->target_temperature_low);
  dump_field(out, "target_temperature_high", this->target_temperature_high);
  dump_field(out, "action", static_cast<enums::ClimateAction>(this->action));
  dump_field(out, "fan_mode", static_cast<enums::ClimateFanMode>(this->fan_mode));
  dump_field(out, "swing_mode", static_cast<enums::ClimateSwingMode>(this->swing_mode));
  dump_field(out, "custom_fan_mode", this->custom_fan_mode);
  dump_field(out, "preset", static_cast<enums::ClimatePreset>(this->preset));
  dump_field(out, "custom_preset", this->custom_preset);
  dump_field(out, "current_humidity", this->current_humidity);
  dump_field(out, "target_humidity", this->target_humidity);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *ClimateCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ClimateCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "has_mode", this->has_mode);
  dump_field(out, "mode", static_cast<enums::ClimateMode>(this->mode));
  dump_field(out, "has_target_temperature", this->has_target_temperature);
  dump_field(out, "target_temperature", this->target_temperature);
  dump_field(out, "has_target_temperature_low", this->has_target_temperature_low);
  dump_field(out, "target_temperature_low", this->target_temperature_low);
  dump_field(out, "has_target_temperature_high", this->has_target_temperature_high);
  dump_field(out, "target_temperature_high", this->target_temperature_high);
  dump_field(out, "has_fan_mode", this->has_fan_mode);
  dump_field(out, "fan_mode", static_cast<enums::ClimateFanMode>(this->fan_mode));
  dump_field(out, "has_swing_mode", this->has_swing_mode);
  dump_field(out, "swing_mode", static_cast<enums::ClimateSwingMode>(this->swing_mode));
  dump_field(out, "has_custom_fan_mode", this->has_custom_fan_mode);
  dump_field(out, "custom_fan_mode", this->custom_fan_mode);
  dump_field(out, "has_preset", this->has_preset);
  dump_field(out, "preset", static_cast<enums::ClimatePreset>(this->preset));
  dump_field(out, "has_custom_preset", this->has_custom_preset);
  dump_field(out, "custom_preset", this->custom_preset);
  dump_field(out, "has_target_humidity", this->has_target_humidity);
  dump_field(out, "target_humidity", this->target_humidity);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_WATER_HEATER
const char *ListEntitiesWaterHeaterResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesWaterHeaterResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  dump_field(out, "min_temperature", this->min_temperature);
  dump_field(out, "max_temperature", this->max_temperature);
  dump_field(out, "target_temperature_step", this->target_temperature_step);
  for (const auto &it : *this->supported_modes) {
    dump_field(out, "supported_modes", static_cast<enums::WaterHeaterMode>(it), 4);
  }
  dump_field(out, "supported_features", this->supported_features);
  return out.c_str();
}
const char *WaterHeaterStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "WaterHeaterStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "current_temperature", this->current_temperature);
  dump_field(out, "target_temperature", this->target_temperature);
  dump_field(out, "mode", static_cast<enums::WaterHeaterMode>(this->mode));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  dump_field(out, "state", this->state);
  dump_field(out, "target_temperature_low", this->target_temperature_low);
  dump_field(out, "target_temperature_high", this->target_temperature_high);
  return out.c_str();
}
const char *WaterHeaterCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "WaterHeaterCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "has_fields", this->has_fields);
  dump_field(out, "mode", static_cast<enums::WaterHeaterMode>(this->mode));
  dump_field(out, "target_temperature", this->target_temperature);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  dump_field(out, "state", this->state);
  dump_field(out, "target_temperature_low", this->target_temperature_low);
  dump_field(out, "target_temperature_high", this->target_temperature_high);
  return out.c_str();
}
#endif
#ifdef USE_NUMBER
const char *ListEntitiesNumberResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesNumberResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "min_value", this->min_value);
  dump_field(out, "max_value", this->max_value);
  dump_field(out, "step", this->step);
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "unit_of_measurement", this->unit_of_measurement);
  dump_field(out, "mode", static_cast<enums::NumberMode>(this->mode));
  dump_field(out, "device_class", this->device_class);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *NumberStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "NumberStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
  dump_field(out, "missing_state", this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *NumberCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "NumberCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_SELECT
const char *ListEntitiesSelectResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesSelectResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  for (const auto &it : *this->options) {
    dump_field(out, "options", it, 4);
  }
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *SelectStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SelectStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
  dump_field(out, "missing_state", this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *SelectCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SelectCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_SIREN
const char *ListEntitiesSirenResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesSirenResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  for (const auto &it : *this->tones) {
    dump_field(out, "tones", it, 4);
  }
  dump_field(out, "supports_duration", this->supports_duration);
  dump_field(out, "supports_volume", this->supports_volume);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *SirenStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SirenStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *SirenCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SirenCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "has_state", this->has_state);
  dump_field(out, "state", this->state);
  dump_field(out, "has_tone", this->has_tone);
  dump_field(out, "tone", this->tone);
  dump_field(out, "has_duration", this->has_duration);
  dump_field(out, "duration", this->duration);
  dump_field(out, "has_volume", this->has_volume);
  dump_field(out, "volume", this->volume);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_LOCK
const char *ListEntitiesLockResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesLockResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "assumed_state", this->assumed_state);
  dump_field(out, "supports_open", this->supports_open);
  dump_field(out, "requires_code", this->requires_code);
  dump_field(out, "code_format", this->code_format);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *LockStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "LockStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", static_cast<enums::LockState>(this->state));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *LockCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "LockCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "command", static_cast<enums::LockCommand>(this->command));
  dump_field(out, "has_code", this->has_code);
  dump_field(out, "code", this->code);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_BUTTON
const char *ListEntitiesButtonResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesButtonResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "device_class", this->device_class);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *ButtonCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ButtonCommandRequest");
  dump_field(out, "key", this->key);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_MEDIA_PLAYER
const char *MediaPlayerSupportedFormat::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "MediaPlayerSupportedFormat");
  dump_field(out, "format", this->format);
  dump_field(out, "sample_rate", this->sample_rate);
  dump_field(out, "num_channels", this->num_channels);
  dump_field(out, "purpose", static_cast<enums::MediaPlayerFormatPurpose>(this->purpose));
  dump_field(out, "sample_bytes", this->sample_bytes);
  return out.c_str();
}
const char *ListEntitiesMediaPlayerResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesMediaPlayerResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "supports_pause", this->supports_pause);
  for (const auto &it : this->supported_formats) {
    out.append("  supported_formats: ");
    it.dump_to(out);
    out.append("\n");
  }
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  dump_field(out, "feature_flags", this->feature_flags);
  return out.c_str();
}
const char *MediaPlayerStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "MediaPlayerStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", static_cast<enums::MediaPlayerState>(this->state));
  dump_field(out, "volume", this->volume);
  dump_field(out, "muted", this->muted);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *MediaPlayerCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "MediaPlayerCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "has_command", this->has_command);
  dump_field(out, "command", static_cast<enums::MediaPlayerCommand>(this->command));
  dump_field(out, "has_volume", this->has_volume);
  dump_field(out, "volume", this->volume);
  dump_field(out, "has_media_url", this->has_media_url);
  dump_field(out, "media_url", this->media_url);
  dump_field(out, "has_announcement", this->has_announcement);
  dump_field(out, "announcement", this->announcement);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_BLUETOOTH_PROXY
const char *SubscribeBluetoothLEAdvertisementsRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SubscribeBluetoothLEAdvertisementsRequest");
  dump_field(out, "flags", this->flags);
  return out.c_str();
}
const char *BluetoothLERawAdvertisement::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothLERawAdvertisement");
  dump_field(out, "address", this->address);
  dump_field(out, "rssi", this->rssi);
  dump_field(out, "address_type", this->address_type);
  dump_bytes_field(out, "data", this->data, this->data_len);
  return out.c_str();
}
const char *BluetoothLERawAdvertisementsResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothLERawAdvertisementsResponse");
  for (uint16_t i = 0; i < this->advertisements_len; i++) {
    out.append("  advertisements: ");
    this->advertisements[i].dump_to(out);
    out.append("\n");
  }
  return out.c_str();
}
const char *BluetoothDeviceRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothDeviceRequest");
  dump_field(out, "address", this->address);
  dump_field(out, "request_type", static_cast<enums::BluetoothDeviceRequestType>(this->request_type));
  dump_field(out, "has_address_type", this->has_address_type);
  dump_field(out, "address_type", this->address_type);
  return out.c_str();
}
const char *BluetoothDeviceConnectionResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothDeviceConnectionResponse");
  dump_field(out, "address", this->address);
  dump_field(out, "connected", this->connected);
  dump_field(out, "mtu", this->mtu);
  dump_field(out, "error", this->error);
  return out.c_str();
}
const char *BluetoothGATTGetServicesRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTGetServicesRequest");
  dump_field(out, "address", this->address);
  return out.c_str();
}
const char *BluetoothGATTDescriptor::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTDescriptor");
  for (const auto &it : this->uuid) {
    dump_field(out, "uuid", it, 4);
  }
  dump_field(out, "handle", this->handle);
  dump_field(out, "short_uuid", this->short_uuid);
  return out.c_str();
}
const char *BluetoothGATTCharacteristic::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTCharacteristic");
  for (const auto &it : this->uuid) {
    dump_field(out, "uuid", it, 4);
  }
  dump_field(out, "handle", this->handle);
  dump_field(out, "properties", this->properties);
  for (const auto &it : this->descriptors) {
    out.append("  descriptors: ");
    it.dump_to(out);
    out.append("\n");
  }
  dump_field(out, "short_uuid", this->short_uuid);
  return out.c_str();
}
const char *BluetoothGATTService::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTService");
  for (const auto &it : this->uuid) {
    dump_field(out, "uuid", it, 4);
  }
  dump_field(out, "handle", this->handle);
  for (const auto &it : this->characteristics) {
    out.append("  characteristics: ");
    it.dump_to(out);
    out.append("\n");
  }
  dump_field(out, "short_uuid", this->short_uuid);
  return out.c_str();
}
const char *BluetoothGATTGetServicesResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTGetServicesResponse");
  dump_field(out, "address", this->address);
  for (const auto &it : this->services) {
    out.append("  services: ");
    it.dump_to(out);
    out.append("\n");
  }
  return out.c_str();
}
const char *BluetoothGATTGetServicesDoneResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTGetServicesDoneResponse");
  dump_field(out, "address", this->address);
  return out.c_str();
}
const char *BluetoothGATTReadRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTReadRequest");
  dump_field(out, "address", this->address);
  dump_field(out, "handle", this->handle);
  return out.c_str();
}
const char *BluetoothGATTReadResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTReadResponse");
  dump_field(out, "address", this->address);
  dump_field(out, "handle", this->handle);
  dump_bytes_field(out, "data", this->data_ptr_, this->data_len_);
  return out.c_str();
}
const char *BluetoothGATTWriteRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTWriteRequest");
  dump_field(out, "address", this->address);
  dump_field(out, "handle", this->handle);
  dump_field(out, "response", this->response);
  dump_bytes_field(out, "data", this->data, this->data_len);
  return out.c_str();
}
const char *BluetoothGATTReadDescriptorRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTReadDescriptorRequest");
  dump_field(out, "address", this->address);
  dump_field(out, "handle", this->handle);
  return out.c_str();
}
const char *BluetoothGATTWriteDescriptorRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTWriteDescriptorRequest");
  dump_field(out, "address", this->address);
  dump_field(out, "handle", this->handle);
  dump_bytes_field(out, "data", this->data, this->data_len);
  return out.c_str();
}
const char *BluetoothGATTNotifyRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTNotifyRequest");
  dump_field(out, "address", this->address);
  dump_field(out, "handle", this->handle);
  dump_field(out, "enable", this->enable);
  return out.c_str();
}
const char *BluetoothGATTNotifyDataResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTNotifyDataResponse");
  dump_field(out, "address", this->address);
  dump_field(out, "handle", this->handle);
  dump_bytes_field(out, "data", this->data_ptr_, this->data_len_);
  return out.c_str();
}
const char *SubscribeBluetoothConnectionsFreeRequest::dump_to(DumpBuffer &out) const {
  out.append("SubscribeBluetoothConnectionsFreeRequest {}");
  return out.c_str();
}
const char *BluetoothConnectionsFreeResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothConnectionsFreeResponse");
  dump_field(out, "free", this->free);
  dump_field(out, "limit", this->limit);
  for (const auto &it : this->allocated) {
    dump_field(out, "allocated", it, 4);
  }
  return out.c_str();
}
const char *BluetoothGATTErrorResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTErrorResponse");
  dump_field(out, "address", this->address);
  dump_field(out, "handle", this->handle);
  dump_field(out, "error", this->error);
  return out.c_str();
}
const char *BluetoothGATTWriteResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTWriteResponse");
  dump_field(out, "address", this->address);
  dump_field(out, "handle", this->handle);
  return out.c_str();
}
const char *BluetoothGATTNotifyResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothGATTNotifyResponse");
  dump_field(out, "address", this->address);
  dump_field(out, "handle", this->handle);
  return out.c_str();
}
const char *BluetoothDevicePairingResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothDevicePairingResponse");
  dump_field(out, "address", this->address);
  dump_field(out, "paired", this->paired);
  dump_field(out, "error", this->error);
  return out.c_str();
}
const char *BluetoothDeviceUnpairingResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothDeviceUnpairingResponse");
  dump_field(out, "address", this->address);
  dump_field(out, "success", this->success);
  dump_field(out, "error", this->error);
  return out.c_str();
}
const char *UnsubscribeBluetoothLEAdvertisementsRequest::dump_to(DumpBuffer &out) const {
  out.append("UnsubscribeBluetoothLEAdvertisementsRequest {}");
  return out.c_str();
}
const char *BluetoothDeviceClearCacheResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothDeviceClearCacheResponse");
  dump_field(out, "address", this->address);
  dump_field(out, "success", this->success);
  dump_field(out, "error", this->error);
  return out.c_str();
}
const char *BluetoothScannerStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothScannerStateResponse");
  dump_field(out, "state", static_cast<enums::BluetoothScannerState>(this->state));
  dump_field(out, "mode", static_cast<enums::BluetoothScannerMode>(this->mode));
  dump_field(out, "configured_mode", static_cast<enums::BluetoothScannerMode>(this->configured_mode));
  return out.c_str();
}
const char *BluetoothScannerSetModeRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "BluetoothScannerSetModeRequest");
  dump_field(out, "mode", static_cast<enums::BluetoothScannerMode>(this->mode));
  return out.c_str();
}
#endif
#ifdef USE_VOICE_ASSISTANT
const char *SubscribeVoiceAssistantRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "SubscribeVoiceAssistantRequest");
  dump_field(out, "subscribe", this->subscribe);
  dump_field(out, "flags", this->flags);
  return out.c_str();
}
const char *VoiceAssistantAudioSettings::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantAudioSettings");
  dump_field(out, "noise_suppression_level", this->noise_suppression_level);
  dump_field(out, "auto_gain", this->auto_gain);
  dump_field(out, "volume_multiplier", this->volume_multiplier);
  return out.c_str();
}
const char *VoiceAssistantRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantRequest");
  dump_field(out, "start", this->start);
  dump_field(out, "conversation_id", this->conversation_id);
  dump_field(out, "flags", this->flags);
  out.append("  audio_settings: ");
  this->audio_settings.dump_to(out);
  out.append("\n");
  dump_field(out, "wake_word_phrase", this->wake_word_phrase);
  return out.c_str();
}
const char *VoiceAssistantResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantResponse");
  dump_field(out, "port", this->port);
  dump_field(out, "error", this->error);
  return out.c_str();
}
const char *VoiceAssistantEventData::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantEventData");
  dump_field(out, "name", this->name);
  dump_field(out, "value", this->value);
  return out.c_str();
}
const char *VoiceAssistantEventResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantEventResponse");
  dump_field(out, "event_type", static_cast<enums::VoiceAssistantEvent>(this->event_type));
  for (const auto &it : this->data) {
    out.append("  data: ");
    it.dump_to(out);
    out.append("\n");
  }
  return out.c_str();
}
const char *VoiceAssistantAudio::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantAudio");
  dump_bytes_field(out, "data", this->data, this->data_len);
  dump_field(out, "end", this->end);
  return out.c_str();
}
const char *VoiceAssistantTimerEventResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantTimerEventResponse");
  dump_field(out, "event_type", static_cast<enums::VoiceAssistantTimerEvent>(this->event_type));
  dump_field(out, "timer_id", this->timer_id);
  dump_field(out, "name", this->name);
  dump_field(out, "total_seconds", this->total_seconds);
  dump_field(out, "seconds_left", this->seconds_left);
  dump_field(out, "is_active", this->is_active);
  return out.c_str();
}
const char *VoiceAssistantAnnounceRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantAnnounceRequest");
  dump_field(out, "media_id", this->media_id);
  dump_field(out, "text", this->text);
  dump_field(out, "preannounce_media_id", this->preannounce_media_id);
  dump_field(out, "start_conversation", this->start_conversation);
  return out.c_str();
}
const char *VoiceAssistantAnnounceFinished::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantAnnounceFinished");
  dump_field(out, "success", this->success);
  return out.c_str();
}
const char *VoiceAssistantWakeWord::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantWakeWord");
  dump_field(out, "id", this->id);
  dump_field(out, "wake_word", this->wake_word);
  for (const auto &it : this->trained_languages) {
    dump_field(out, "trained_languages", it, 4);
  }
  return out.c_str();
}
const char *VoiceAssistantExternalWakeWord::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantExternalWakeWord");
  dump_field(out, "id", this->id);
  dump_field(out, "wake_word", this->wake_word);
  for (const auto &it : this->trained_languages) {
    dump_field(out, "trained_languages", it, 4);
  }
  dump_field(out, "model_type", this->model_type);
  dump_field(out, "model_size", this->model_size);
  dump_field(out, "model_hash", this->model_hash);
  dump_field(out, "url", this->url);
  return out.c_str();
}
const char *VoiceAssistantConfigurationRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantConfigurationRequest");
  for (const auto &it : this->external_wake_words) {
    out.append("  external_wake_words: ");
    it.dump_to(out);
    out.append("\n");
  }
  return out.c_str();
}
const char *VoiceAssistantConfigurationResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantConfigurationResponse");
  for (const auto &it : this->available_wake_words) {
    out.append("  available_wake_words: ");
    it.dump_to(out);
    out.append("\n");
  }
  for (const auto &it : *this->active_wake_words) {
    dump_field(out, "active_wake_words", it, 4);
  }
  dump_field(out, "max_active_wake_words", this->max_active_wake_words);
  return out.c_str();
}
const char *VoiceAssistantSetConfiguration::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "VoiceAssistantSetConfiguration");
  for (const auto &it : this->active_wake_words) {
    dump_field(out, "active_wake_words", it, 4);
  }
  return out.c_str();
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
const char *ListEntitiesAlarmControlPanelResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesAlarmControlPanelResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "supported_features", this->supported_features);
  dump_field(out, "requires_code", this->requires_code);
  dump_field(out, "requires_code_to_arm", this->requires_code_to_arm);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *AlarmControlPanelStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "AlarmControlPanelStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", static_cast<enums::AlarmControlPanelState>(this->state));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *AlarmControlPanelCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "AlarmControlPanelCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "command", static_cast<enums::AlarmControlPanelStateCommand>(this->command));
  dump_field(out, "code", this->code);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_TEXT
const char *ListEntitiesTextResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesTextResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "min_length", this->min_length);
  dump_field(out, "max_length", this->max_length);
  dump_field(out, "pattern", this->pattern);
  dump_field(out, "mode", static_cast<enums::TextMode>(this->mode));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *TextStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "TextStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
  dump_field(out, "missing_state", this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *TextCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "TextCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "state", this->state);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_DATETIME_DATE
const char *ListEntitiesDateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesDateResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *DateStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "DateStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "missing_state", this->missing_state);
  dump_field(out, "year", this->year);
  dump_field(out, "month", this->month);
  dump_field(out, "day", this->day);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *DateCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "DateCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "year", this->year);
  dump_field(out, "month", this->month);
  dump_field(out, "day", this->day);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_DATETIME_TIME
const char *ListEntitiesTimeResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesTimeResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *TimeStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "TimeStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "missing_state", this->missing_state);
  dump_field(out, "hour", this->hour);
  dump_field(out, "minute", this->minute);
  dump_field(out, "second", this->second);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *TimeCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "TimeCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "hour", this->hour);
  dump_field(out, "minute", this->minute);
  dump_field(out, "second", this->second);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_EVENT
const char *ListEntitiesEventResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesEventResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "device_class", this->device_class);
  for (const auto &it : *this->event_types) {
    dump_field(out, "event_types", it, 4);
  }
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *EventResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "EventResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "event_type", this->event_type);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_VALVE
const char *ListEntitiesValveResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesValveResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "device_class", this->device_class);
  dump_field(out, "assumed_state", this->assumed_state);
  dump_field(out, "supports_position", this->supports_position);
  dump_field(out, "supports_stop", this->supports_stop);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *ValveStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ValveStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "position", this->position);
  dump_field(out, "current_operation", static_cast<enums::ValveOperation>(this->current_operation));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *ValveCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ValveCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "has_position", this->has_position);
  dump_field(out, "position", this->position);
  dump_field(out, "stop", this->stop);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_DATETIME_DATETIME
const char *ListEntitiesDateTimeResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesDateTimeResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *DateTimeStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "DateTimeStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "missing_state", this->missing_state);
  dump_field(out, "epoch_seconds", this->epoch_seconds);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *DateTimeCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "DateTimeCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "epoch_seconds", this->epoch_seconds);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_UPDATE
const char *ListEntitiesUpdateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesUpdateResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, "device_class", this->device_class);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *UpdateStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "UpdateStateResponse");
  dump_field(out, "key", this->key);
  dump_field(out, "missing_state", this->missing_state);
  dump_field(out, "in_progress", this->in_progress);
  dump_field(out, "has_progress", this->has_progress);
  dump_field(out, "progress", this->progress);
  dump_field(out, "current_version", this->current_version);
  dump_field(out, "latest_version", this->latest_version);
  dump_field(out, "title", this->title);
  dump_field(out, "release_summary", this->release_summary);
  dump_field(out, "release_url", this->release_url);
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
const char *UpdateCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "UpdateCommandRequest");
  dump_field(out, "key", this->key);
  dump_field(out, "command", static_cast<enums::UpdateCommand>(this->command));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_ZWAVE_PROXY
const char *ZWaveProxyFrame::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ZWaveProxyFrame");
  dump_bytes_field(out, "data", this->data, this->data_len);
  return out.c_str();
}
const char *ZWaveProxyRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ZWaveProxyRequest");
  dump_field(out, "type", static_cast<enums::ZWaveProxyRequestType>(this->type));
  dump_bytes_field(out, "data", this->data, this->data_len);
  return out.c_str();
}
#endif
#ifdef USE_INFRARED
const char *ListEntitiesInfraredResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "ListEntitiesInfraredResponse");
  dump_field(out, "object_id", this->object_id);
  dump_field(out, "key", this->key);
  dump_field(out, "name", this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, "icon", this->icon);
#endif
  dump_field(out, "disabled_by_default", this->disabled_by_default);
  dump_field(out, "entity_category", static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  dump_field(out, "capabilities", this->capabilities);
  return out.c_str();
}
#endif
#ifdef USE_IR_RF
const char *InfraredRFTransmitRawTimingsRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "InfraredRFTransmitRawTimingsRequest");
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  dump_field(out, "key", this->key);
  dump_field(out, "carrier_frequency", this->carrier_frequency);
  dump_field(out, "repeat_count", this->repeat_count);
  out.append("  timings: ");
  out.append("packed buffer [");
  append_uint(out, this->timings_count_);
  out.append(" values, ");
  append_uint(out, this->timings_length_);
  out.append(" bytes]\n");
  return out.c_str();
}
const char *InfraredRFReceiveEvent::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, "InfraredRFReceiveEvent");
#ifdef USE_DEVICES
  dump_field(out, "device_id", this->device_id);
#endif
  dump_field(out, "key", this->key);
  for (const auto &it : *this->timings) {
    dump_field(out, "timings", it, 4);
  }
  return out.c_str();
}
#endif

}  // namespace esphome::api

#endif  // HAS_PROTO_MESSAGE_DUMP
