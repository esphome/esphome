// This file was automatically generated with a tool.
// See script/api_protobuf/api_protobuf.py
#include "api_pb2.h"
#include "esphome/core/helpers.h"
#include "esphome/core/progmem.h"

#include <cinttypes>

#ifdef HAS_PROTO_MESSAGE_DUMP

namespace esphome::api {

#ifdef USE_ESP8266
// Out-of-line to avoid inlining strlen_P/memcpy_P at every call site
void DumpBuffer::append_p_esp8266(const char *str) {
  size_t len = strlen_P(str);
  size_t space = CAPACITY - 1 - pos_;
  if (len > space)
    len = space;
  if (len > 0) {
    memcpy_P(buf_ + pos_, str, len);
    pos_ += len;
    buf_[pos_] = '\0';
  }
}
#endif

// Helper function to append a quoted string, handling empty StringRef
static inline void append_quoted_string(DumpBuffer &out, const StringRef &ref) {
  out.append("'");
  if (!ref.empty()) {
    out.append(ref.c_str(), ref.size());
  }
  out.append("'");
}

// Common helpers for dump_field functions
// field_name is a PROGMEM pointer (flash on ESP8266, regular pointer on other platforms)
static inline void append_field_prefix(DumpBuffer &out, const char *field_name, int indent) {
  out.append(indent, ' ').append_p(field_name).append(": ");
}

static inline void append_uint(DumpBuffer &out, uint32_t value) {
  out.set_pos(buf_append_printf(out.data(), DumpBuffer::CAPACITY, out.pos(), "%" PRIu32, value));
}

// RAII helper for message dump formatting
// message_name is a PROGMEM pointer (flash on ESP8266, regular pointer on other platforms)
class MessageDumpHelper {
 public:
  MessageDumpHelper(DumpBuffer &out, const char *message_name) : out_(out) {
    out_.append_p(message_name);
    out_.append(" {\n");
  }
  ~MessageDumpHelper() { out_.append(" }"); }

 private:
  DumpBuffer &out_;
};

// Helper functions to reduce code duplication in dump methods
// field_name parameters are PROGMEM pointers (flash on ESP8266, regular pointers on other platforms)
// Not all overloads are used in every build (depends on enabled components)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
static void dump_field(DumpBuffer &out, const char *field_name, int32_t value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.set_pos(buf_append_printf(out.data(), DumpBuffer::CAPACITY, out.pos(), "%" PRId32 "\n", value));
}

static void dump_field(DumpBuffer &out, const char *field_name, uint32_t value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.set_pos(buf_append_printf(out.data(), DumpBuffer::CAPACITY, out.pos(), "%" PRIu32 "\n", value));
}

static void dump_field(DumpBuffer &out, const char *field_name, float value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.set_pos(buf_append_printf(out.data(), DumpBuffer::CAPACITY, out.pos(), "%g\n", value));
}

static void dump_field(DumpBuffer &out, const char *field_name, uint64_t value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.set_pos(buf_append_printf(out.data(), DumpBuffer::CAPACITY, out.pos(), "%" PRIu64 "\n", value));
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

// proto_enum_to_string returns PROGMEM pointers, so use append_p
template<typename T> static void dump_field(DumpBuffer &out, const char *field_name, T value, int indent = 2) {
  append_field_prefix(out, field_name, indent);
  out.append_p(proto_enum_to_string<T>(value));
  out.append("\n");
}

// Helper for bytes fields - uses stack buffer to avoid heap allocation
// Buffer sized for 160 bytes of data (480 chars with separators) to fit typical log buffer
// field_name is a PROGMEM pointer (flash on ESP8266, regular pointer on other platforms)
static void dump_bytes_field(DumpBuffer &out, const char *field_name, const uint8_t *data, size_t len, int indent = 2) {
  char hex_buf[format_hex_pretty_size(160)];
  append_field_prefix(out, field_name, indent);
  format_hex_pretty_to(hex_buf, data, len);
  out.append(hex_buf).append("\n");
}
#pragma GCC diagnostic pop

template<> const char *proto_enum_to_string<enums::SerialProxyPortType>(enums::SerialProxyPortType value) {
  switch (value) {
    case enums::SERIAL_PROXY_PORT_TYPE_TTL:
      return ESPHOME_PSTR("SERIAL_PROXY_PORT_TYPE_TTL");
    case enums::SERIAL_PROXY_PORT_TYPE_RS232:
      return ESPHOME_PSTR("SERIAL_PROXY_PORT_TYPE_RS232");
    case enums::SERIAL_PROXY_PORT_TYPE_RS485:
      return ESPHOME_PSTR("SERIAL_PROXY_PORT_TYPE_RS485");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::EntityCategory>(enums::EntityCategory value) {
  switch (value) {
    case enums::ENTITY_CATEGORY_NONE:
      return ESPHOME_PSTR("ENTITY_CATEGORY_NONE");
    case enums::ENTITY_CATEGORY_CONFIG:
      return ESPHOME_PSTR("ENTITY_CATEGORY_CONFIG");
    case enums::ENTITY_CATEGORY_DIAGNOSTIC:
      return ESPHOME_PSTR("ENTITY_CATEGORY_DIAGNOSTIC");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#ifdef USE_COVER
template<> const char *proto_enum_to_string<enums::CoverOperation>(enums::CoverOperation value) {
  switch (value) {
    case enums::COVER_OPERATION_IDLE:
      return ESPHOME_PSTR("COVER_OPERATION_IDLE");
    case enums::COVER_OPERATION_IS_OPENING:
      return ESPHOME_PSTR("COVER_OPERATION_IS_OPENING");
    case enums::COVER_OPERATION_IS_CLOSING:
      return ESPHOME_PSTR("COVER_OPERATION_IS_CLOSING");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_FAN
template<> const char *proto_enum_to_string<enums::FanDirection>(enums::FanDirection value) {
  switch (value) {
    case enums::FAN_DIRECTION_FORWARD:
      return ESPHOME_PSTR("FAN_DIRECTION_FORWARD");
    case enums::FAN_DIRECTION_REVERSE:
      return ESPHOME_PSTR("FAN_DIRECTION_REVERSE");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_LIGHT
template<> const char *proto_enum_to_string<enums::ColorMode>(enums::ColorMode value) {
  switch (value) {
    case enums::COLOR_MODE_UNKNOWN:
      return ESPHOME_PSTR("COLOR_MODE_UNKNOWN");
    case enums::COLOR_MODE_ON_OFF:
      return ESPHOME_PSTR("COLOR_MODE_ON_OFF");
    case enums::COLOR_MODE_LEGACY_BRIGHTNESS:
      return ESPHOME_PSTR("COLOR_MODE_LEGACY_BRIGHTNESS");
    case enums::COLOR_MODE_BRIGHTNESS:
      return ESPHOME_PSTR("COLOR_MODE_BRIGHTNESS");
    case enums::COLOR_MODE_WHITE:
      return ESPHOME_PSTR("COLOR_MODE_WHITE");
    case enums::COLOR_MODE_COLOR_TEMPERATURE:
      return ESPHOME_PSTR("COLOR_MODE_COLOR_TEMPERATURE");
    case enums::COLOR_MODE_COLD_WARM_WHITE:
      return ESPHOME_PSTR("COLOR_MODE_COLD_WARM_WHITE");
    case enums::COLOR_MODE_RGB:
      return ESPHOME_PSTR("COLOR_MODE_RGB");
    case enums::COLOR_MODE_RGB_WHITE:
      return ESPHOME_PSTR("COLOR_MODE_RGB_WHITE");
    case enums::COLOR_MODE_RGB_COLOR_TEMPERATURE:
      return ESPHOME_PSTR("COLOR_MODE_RGB_COLOR_TEMPERATURE");
    case enums::COLOR_MODE_RGB_COLD_WARM_WHITE:
      return ESPHOME_PSTR("COLOR_MODE_RGB_COLD_WARM_WHITE");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_SENSOR
template<> const char *proto_enum_to_string<enums::SensorStateClass>(enums::SensorStateClass value) {
  switch (value) {
    case enums::STATE_CLASS_NONE:
      return ESPHOME_PSTR("STATE_CLASS_NONE");
    case enums::STATE_CLASS_MEASUREMENT:
      return ESPHOME_PSTR("STATE_CLASS_MEASUREMENT");
    case enums::STATE_CLASS_TOTAL_INCREASING:
      return ESPHOME_PSTR("STATE_CLASS_TOTAL_INCREASING");
    case enums::STATE_CLASS_TOTAL:
      return ESPHOME_PSTR("STATE_CLASS_TOTAL");
    case enums::STATE_CLASS_MEASUREMENT_ANGLE:
      return ESPHOME_PSTR("STATE_CLASS_MEASUREMENT_ANGLE");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
template<> const char *proto_enum_to_string<enums::LogLevel>(enums::LogLevel value) {
  switch (value) {
    case enums::LOG_LEVEL_NONE:
      return ESPHOME_PSTR("LOG_LEVEL_NONE");
    case enums::LOG_LEVEL_ERROR:
      return ESPHOME_PSTR("LOG_LEVEL_ERROR");
    case enums::LOG_LEVEL_WARN:
      return ESPHOME_PSTR("LOG_LEVEL_WARN");
    case enums::LOG_LEVEL_INFO:
      return ESPHOME_PSTR("LOG_LEVEL_INFO");
    case enums::LOG_LEVEL_CONFIG:
      return ESPHOME_PSTR("LOG_LEVEL_CONFIG");
    case enums::LOG_LEVEL_DEBUG:
      return ESPHOME_PSTR("LOG_LEVEL_DEBUG");
    case enums::LOG_LEVEL_VERBOSE:
      return ESPHOME_PSTR("LOG_LEVEL_VERBOSE");
    case enums::LOG_LEVEL_VERY_VERBOSE:
      return ESPHOME_PSTR("LOG_LEVEL_VERY_VERBOSE");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::DSTRuleType>(enums::DSTRuleType value) {
  switch (value) {
    case enums::DST_RULE_TYPE_NONE:
      return ESPHOME_PSTR("DST_RULE_TYPE_NONE");
    case enums::DST_RULE_TYPE_MONTH_WEEK_DAY:
      return ESPHOME_PSTR("DST_RULE_TYPE_MONTH_WEEK_DAY");
    case enums::DST_RULE_TYPE_JULIAN_NO_LEAP:
      return ESPHOME_PSTR("DST_RULE_TYPE_JULIAN_NO_LEAP");
    case enums::DST_RULE_TYPE_DAY_OF_YEAR:
      return ESPHOME_PSTR("DST_RULE_TYPE_DAY_OF_YEAR");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#ifdef USE_API_USER_DEFINED_ACTIONS
template<> const char *proto_enum_to_string<enums::ServiceArgType>(enums::ServiceArgType value) {
  switch (value) {
    case enums::SERVICE_ARG_TYPE_BOOL:
      return ESPHOME_PSTR("SERVICE_ARG_TYPE_BOOL");
    case enums::SERVICE_ARG_TYPE_INT:
      return ESPHOME_PSTR("SERVICE_ARG_TYPE_INT");
    case enums::SERVICE_ARG_TYPE_FLOAT:
      return ESPHOME_PSTR("SERVICE_ARG_TYPE_FLOAT");
    case enums::SERVICE_ARG_TYPE_STRING:
      return ESPHOME_PSTR("SERVICE_ARG_TYPE_STRING");
    case enums::SERVICE_ARG_TYPE_BOOL_ARRAY:
      return ESPHOME_PSTR("SERVICE_ARG_TYPE_BOOL_ARRAY");
    case enums::SERVICE_ARG_TYPE_INT_ARRAY:
      return ESPHOME_PSTR("SERVICE_ARG_TYPE_INT_ARRAY");
    case enums::SERVICE_ARG_TYPE_FLOAT_ARRAY:
      return ESPHOME_PSTR("SERVICE_ARG_TYPE_FLOAT_ARRAY");
    case enums::SERVICE_ARG_TYPE_STRING_ARRAY:
      return ESPHOME_PSTR("SERVICE_ARG_TYPE_STRING_ARRAY");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::SupportsResponseType>(enums::SupportsResponseType value) {
  switch (value) {
    case enums::SUPPORTS_RESPONSE_NONE:
      return ESPHOME_PSTR("SUPPORTS_RESPONSE_NONE");
    case enums::SUPPORTS_RESPONSE_OPTIONAL:
      return ESPHOME_PSTR("SUPPORTS_RESPONSE_OPTIONAL");
    case enums::SUPPORTS_RESPONSE_ONLY:
      return ESPHOME_PSTR("SUPPORTS_RESPONSE_ONLY");
    case enums::SUPPORTS_RESPONSE_STATUS:
      return ESPHOME_PSTR("SUPPORTS_RESPONSE_STATUS");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
template<> const char *proto_enum_to_string<enums::TemperatureUnit>(enums::TemperatureUnit value) {
  switch (value) {
    case enums::TEMPERATURE_UNIT_CELSIUS:
      return ESPHOME_PSTR("TEMPERATURE_UNIT_CELSIUS");
    case enums::TEMPERATURE_UNIT_FAHRENHEIT:
      return ESPHOME_PSTR("TEMPERATURE_UNIT_FAHRENHEIT");
    case enums::TEMPERATURE_UNIT_KELVIN:
      return ESPHOME_PSTR("TEMPERATURE_UNIT_KELVIN");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#ifdef USE_CLIMATE
template<> const char *proto_enum_to_string<enums::ClimateMode>(enums::ClimateMode value) {
  switch (value) {
    case enums::CLIMATE_MODE_OFF:
      return ESPHOME_PSTR("CLIMATE_MODE_OFF");
    case enums::CLIMATE_MODE_HEAT_COOL:
      return ESPHOME_PSTR("CLIMATE_MODE_HEAT_COOL");
    case enums::CLIMATE_MODE_COOL:
      return ESPHOME_PSTR("CLIMATE_MODE_COOL");
    case enums::CLIMATE_MODE_HEAT:
      return ESPHOME_PSTR("CLIMATE_MODE_HEAT");
    case enums::CLIMATE_MODE_FAN_ONLY:
      return ESPHOME_PSTR("CLIMATE_MODE_FAN_ONLY");
    case enums::CLIMATE_MODE_DRY:
      return ESPHOME_PSTR("CLIMATE_MODE_DRY");
    case enums::CLIMATE_MODE_AUTO:
      return ESPHOME_PSTR("CLIMATE_MODE_AUTO");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::ClimateFanMode>(enums::ClimateFanMode value) {
  switch (value) {
    case enums::CLIMATE_FAN_ON:
      return ESPHOME_PSTR("CLIMATE_FAN_ON");
    case enums::CLIMATE_FAN_OFF:
      return ESPHOME_PSTR("CLIMATE_FAN_OFF");
    case enums::CLIMATE_FAN_AUTO:
      return ESPHOME_PSTR("CLIMATE_FAN_AUTO");
    case enums::CLIMATE_FAN_LOW:
      return ESPHOME_PSTR("CLIMATE_FAN_LOW");
    case enums::CLIMATE_FAN_MEDIUM:
      return ESPHOME_PSTR("CLIMATE_FAN_MEDIUM");
    case enums::CLIMATE_FAN_HIGH:
      return ESPHOME_PSTR("CLIMATE_FAN_HIGH");
    case enums::CLIMATE_FAN_MIDDLE:
      return ESPHOME_PSTR("CLIMATE_FAN_MIDDLE");
    case enums::CLIMATE_FAN_FOCUS:
      return ESPHOME_PSTR("CLIMATE_FAN_FOCUS");
    case enums::CLIMATE_FAN_DIFFUSE:
      return ESPHOME_PSTR("CLIMATE_FAN_DIFFUSE");
    case enums::CLIMATE_FAN_QUIET:
      return ESPHOME_PSTR("CLIMATE_FAN_QUIET");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::ClimateSwingMode>(enums::ClimateSwingMode value) {
  switch (value) {
    case enums::CLIMATE_SWING_OFF:
      return ESPHOME_PSTR("CLIMATE_SWING_OFF");
    case enums::CLIMATE_SWING_BOTH:
      return ESPHOME_PSTR("CLIMATE_SWING_BOTH");
    case enums::CLIMATE_SWING_VERTICAL:
      return ESPHOME_PSTR("CLIMATE_SWING_VERTICAL");
    case enums::CLIMATE_SWING_HORIZONTAL:
      return ESPHOME_PSTR("CLIMATE_SWING_HORIZONTAL");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::ClimateAction>(enums::ClimateAction value) {
  switch (value) {
    case enums::CLIMATE_ACTION_OFF:
      return ESPHOME_PSTR("CLIMATE_ACTION_OFF");
    case enums::CLIMATE_ACTION_COOLING:
      return ESPHOME_PSTR("CLIMATE_ACTION_COOLING");
    case enums::CLIMATE_ACTION_HEATING:
      return ESPHOME_PSTR("CLIMATE_ACTION_HEATING");
    case enums::CLIMATE_ACTION_IDLE:
      return ESPHOME_PSTR("CLIMATE_ACTION_IDLE");
    case enums::CLIMATE_ACTION_DRYING:
      return ESPHOME_PSTR("CLIMATE_ACTION_DRYING");
    case enums::CLIMATE_ACTION_FAN:
      return ESPHOME_PSTR("CLIMATE_ACTION_FAN");
    case enums::CLIMATE_ACTION_DEFROSTING:
      return ESPHOME_PSTR("CLIMATE_ACTION_DEFROSTING");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::ClimatePreset>(enums::ClimatePreset value) {
  switch (value) {
    case enums::CLIMATE_PRESET_NONE:
      return ESPHOME_PSTR("CLIMATE_PRESET_NONE");
    case enums::CLIMATE_PRESET_HOME:
      return ESPHOME_PSTR("CLIMATE_PRESET_HOME");
    case enums::CLIMATE_PRESET_AWAY:
      return ESPHOME_PSTR("CLIMATE_PRESET_AWAY");
    case enums::CLIMATE_PRESET_BOOST:
      return ESPHOME_PSTR("CLIMATE_PRESET_BOOST");
    case enums::CLIMATE_PRESET_COMFORT:
      return ESPHOME_PSTR("CLIMATE_PRESET_COMFORT");
    case enums::CLIMATE_PRESET_ECO:
      return ESPHOME_PSTR("CLIMATE_PRESET_ECO");
    case enums::CLIMATE_PRESET_SLEEP:
      return ESPHOME_PSTR("CLIMATE_PRESET_SLEEP");
    case enums::CLIMATE_PRESET_ACTIVITY:
      return ESPHOME_PSTR("CLIMATE_PRESET_ACTIVITY");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_WATER_HEATER
template<> const char *proto_enum_to_string<enums::WaterHeaterMode>(enums::WaterHeaterMode value) {
  switch (value) {
    case enums::WATER_HEATER_MODE_OFF:
      return ESPHOME_PSTR("WATER_HEATER_MODE_OFF");
    case enums::WATER_HEATER_MODE_ECO:
      return ESPHOME_PSTR("WATER_HEATER_MODE_ECO");
    case enums::WATER_HEATER_MODE_ELECTRIC:
      return ESPHOME_PSTR("WATER_HEATER_MODE_ELECTRIC");
    case enums::WATER_HEATER_MODE_PERFORMANCE:
      return ESPHOME_PSTR("WATER_HEATER_MODE_PERFORMANCE");
    case enums::WATER_HEATER_MODE_HIGH_DEMAND:
      return ESPHOME_PSTR("WATER_HEATER_MODE_HIGH_DEMAND");
    case enums::WATER_HEATER_MODE_HEAT_PUMP:
      return ESPHOME_PSTR("WATER_HEATER_MODE_HEAT_PUMP");
    case enums::WATER_HEATER_MODE_GAS:
      return ESPHOME_PSTR("WATER_HEATER_MODE_GAS");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
template<>
const char *proto_enum_to_string<enums::WaterHeaterCommandHasField>(enums::WaterHeaterCommandHasField value) {
  switch (value) {
    case enums::WATER_HEATER_COMMAND_HAS_NONE:
      return ESPHOME_PSTR("WATER_HEATER_COMMAND_HAS_NONE");
    case enums::WATER_HEATER_COMMAND_HAS_MODE:
      return ESPHOME_PSTR("WATER_HEATER_COMMAND_HAS_MODE");
    case enums::WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE:
      return ESPHOME_PSTR("WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE");
    case enums::WATER_HEATER_COMMAND_HAS_STATE:
      return ESPHOME_PSTR("WATER_HEATER_COMMAND_HAS_STATE");
    case enums::WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE_LOW:
      return ESPHOME_PSTR("WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE_LOW");
    case enums::WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE_HIGH:
      return ESPHOME_PSTR("WATER_HEATER_COMMAND_HAS_TARGET_TEMPERATURE_HIGH");
    case enums::WATER_HEATER_COMMAND_HAS_ON_STATE:
      return ESPHOME_PSTR("WATER_HEATER_COMMAND_HAS_ON_STATE");
    case enums::WATER_HEATER_COMMAND_HAS_AWAY_STATE:
      return ESPHOME_PSTR("WATER_HEATER_COMMAND_HAS_AWAY_STATE");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#ifdef USE_NUMBER
template<> const char *proto_enum_to_string<enums::NumberMode>(enums::NumberMode value) {
  switch (value) {
    case enums::NUMBER_MODE_AUTO:
      return ESPHOME_PSTR("NUMBER_MODE_AUTO");
    case enums::NUMBER_MODE_BOX:
      return ESPHOME_PSTR("NUMBER_MODE_BOX");
    case enums::NUMBER_MODE_SLIDER:
      return ESPHOME_PSTR("NUMBER_MODE_SLIDER");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_LOCK
template<> const char *proto_enum_to_string<enums::LockState>(enums::LockState value) {
  switch (value) {
    case enums::LOCK_STATE_NONE:
      return ESPHOME_PSTR("LOCK_STATE_NONE");
    case enums::LOCK_STATE_LOCKED:
      return ESPHOME_PSTR("LOCK_STATE_LOCKED");
    case enums::LOCK_STATE_UNLOCKED:
      return ESPHOME_PSTR("LOCK_STATE_UNLOCKED");
    case enums::LOCK_STATE_JAMMED:
      return ESPHOME_PSTR("LOCK_STATE_JAMMED");
    case enums::LOCK_STATE_LOCKING:
      return ESPHOME_PSTR("LOCK_STATE_LOCKING");
    case enums::LOCK_STATE_UNLOCKING:
      return ESPHOME_PSTR("LOCK_STATE_UNLOCKING");
    case enums::LOCK_STATE_OPENING:
      return ESPHOME_PSTR("LOCK_STATE_OPENING");
    case enums::LOCK_STATE_OPEN:
      return ESPHOME_PSTR("LOCK_STATE_OPEN");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::LockCommand>(enums::LockCommand value) {
  switch (value) {
    case enums::LOCK_UNLOCK:
      return ESPHOME_PSTR("LOCK_UNLOCK");
    case enums::LOCK_LOCK:
      return ESPHOME_PSTR("LOCK_LOCK");
    case enums::LOCK_OPEN:
      return ESPHOME_PSTR("LOCK_OPEN");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_MEDIA_PLAYER
template<> const char *proto_enum_to_string<enums::MediaPlayerState>(enums::MediaPlayerState value) {
  switch (value) {
    case enums::MEDIA_PLAYER_STATE_NONE:
      return ESPHOME_PSTR("MEDIA_PLAYER_STATE_NONE");
    case enums::MEDIA_PLAYER_STATE_IDLE:
      return ESPHOME_PSTR("MEDIA_PLAYER_STATE_IDLE");
    case enums::MEDIA_PLAYER_STATE_PLAYING:
      return ESPHOME_PSTR("MEDIA_PLAYER_STATE_PLAYING");
    case enums::MEDIA_PLAYER_STATE_PAUSED:
      return ESPHOME_PSTR("MEDIA_PLAYER_STATE_PAUSED");
    case enums::MEDIA_PLAYER_STATE_ANNOUNCING:
      return ESPHOME_PSTR("MEDIA_PLAYER_STATE_ANNOUNCING");
    case enums::MEDIA_PLAYER_STATE_OFF:
      return ESPHOME_PSTR("MEDIA_PLAYER_STATE_OFF");
    case enums::MEDIA_PLAYER_STATE_ON:
      return ESPHOME_PSTR("MEDIA_PLAYER_STATE_ON");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::MediaPlayerCommand>(enums::MediaPlayerCommand value) {
  switch (value) {
    case enums::MEDIA_PLAYER_COMMAND_PLAY:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_PLAY");
    case enums::MEDIA_PLAYER_COMMAND_PAUSE:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_PAUSE");
    case enums::MEDIA_PLAYER_COMMAND_STOP:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_STOP");
    case enums::MEDIA_PLAYER_COMMAND_MUTE:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_MUTE");
    case enums::MEDIA_PLAYER_COMMAND_UNMUTE:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_UNMUTE");
    case enums::MEDIA_PLAYER_COMMAND_TOGGLE:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_TOGGLE");
    case enums::MEDIA_PLAYER_COMMAND_VOLUME_UP:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_VOLUME_UP");
    case enums::MEDIA_PLAYER_COMMAND_VOLUME_DOWN:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_VOLUME_DOWN");
    case enums::MEDIA_PLAYER_COMMAND_ENQUEUE:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_ENQUEUE");
    case enums::MEDIA_PLAYER_COMMAND_REPEAT_ONE:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_REPEAT_ONE");
    case enums::MEDIA_PLAYER_COMMAND_REPEAT_OFF:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_REPEAT_OFF");
    case enums::MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_CLEAR_PLAYLIST");
    case enums::MEDIA_PLAYER_COMMAND_TURN_ON:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_TURN_ON");
    case enums::MEDIA_PLAYER_COMMAND_TURN_OFF:
      return ESPHOME_PSTR("MEDIA_PLAYER_COMMAND_TURN_OFF");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::MediaPlayerFormatPurpose>(enums::MediaPlayerFormatPurpose value) {
  switch (value) {
    case enums::MEDIA_PLAYER_FORMAT_PURPOSE_DEFAULT:
      return ESPHOME_PSTR("MEDIA_PLAYER_FORMAT_PURPOSE_DEFAULT");
    case enums::MEDIA_PLAYER_FORMAT_PURPOSE_ANNOUNCEMENT:
      return ESPHOME_PSTR("MEDIA_PLAYER_FORMAT_PURPOSE_ANNOUNCEMENT");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_BLUETOOTH_PROXY
template<>
const char *proto_enum_to_string<enums::BluetoothDeviceRequestType>(enums::BluetoothDeviceRequestType value) {
  switch (value) {
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT:
      return ESPHOME_PSTR("BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT");
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_DISCONNECT:
      return ESPHOME_PSTR("BLUETOOTH_DEVICE_REQUEST_TYPE_DISCONNECT");
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR:
      return ESPHOME_PSTR("BLUETOOTH_DEVICE_REQUEST_TYPE_PAIR");
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR:
      return ESPHOME_PSTR("BLUETOOTH_DEVICE_REQUEST_TYPE_UNPAIR");
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE:
      return ESPHOME_PSTR("BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITH_CACHE");
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE:
      return ESPHOME_PSTR("BLUETOOTH_DEVICE_REQUEST_TYPE_CONNECT_V3_WITHOUT_CACHE");
    case enums::BLUETOOTH_DEVICE_REQUEST_TYPE_CLEAR_CACHE:
      return ESPHOME_PSTR("BLUETOOTH_DEVICE_REQUEST_TYPE_CLEAR_CACHE");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::BluetoothScannerState>(enums::BluetoothScannerState value) {
  switch (value) {
    case enums::BLUETOOTH_SCANNER_STATE_IDLE:
      return ESPHOME_PSTR("BLUETOOTH_SCANNER_STATE_IDLE");
    case enums::BLUETOOTH_SCANNER_STATE_STARTING:
      return ESPHOME_PSTR("BLUETOOTH_SCANNER_STATE_STARTING");
    case enums::BLUETOOTH_SCANNER_STATE_RUNNING:
      return ESPHOME_PSTR("BLUETOOTH_SCANNER_STATE_RUNNING");
    case enums::BLUETOOTH_SCANNER_STATE_FAILED:
      return ESPHOME_PSTR("BLUETOOTH_SCANNER_STATE_FAILED");
    case enums::BLUETOOTH_SCANNER_STATE_STOPPING:
      return ESPHOME_PSTR("BLUETOOTH_SCANNER_STATE_STOPPING");
    case enums::BLUETOOTH_SCANNER_STATE_STOPPED:
      return ESPHOME_PSTR("BLUETOOTH_SCANNER_STATE_STOPPED");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::BluetoothScannerMode>(enums::BluetoothScannerMode value) {
  switch (value) {
    case enums::BLUETOOTH_SCANNER_MODE_PASSIVE:
      return ESPHOME_PSTR("BLUETOOTH_SCANNER_MODE_PASSIVE");
    case enums::BLUETOOTH_SCANNER_MODE_ACTIVE:
      return ESPHOME_PSTR("BLUETOOTH_SCANNER_MODE_ACTIVE");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
template<>
const char *proto_enum_to_string<enums::VoiceAssistantSubscribeFlag>(enums::VoiceAssistantSubscribeFlag value) {
  switch (value) {
    case enums::VOICE_ASSISTANT_SUBSCRIBE_NONE:
      return ESPHOME_PSTR("VOICE_ASSISTANT_SUBSCRIBE_NONE");
    case enums::VOICE_ASSISTANT_SUBSCRIBE_API_AUDIO:
      return ESPHOME_PSTR("VOICE_ASSISTANT_SUBSCRIBE_API_AUDIO");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::VoiceAssistantRequestFlag>(enums::VoiceAssistantRequestFlag value) {
  switch (value) {
    case enums::VOICE_ASSISTANT_REQUEST_NONE:
      return ESPHOME_PSTR("VOICE_ASSISTANT_REQUEST_NONE");
    case enums::VOICE_ASSISTANT_REQUEST_USE_VAD:
      return ESPHOME_PSTR("VOICE_ASSISTANT_REQUEST_USE_VAD");
    case enums::VOICE_ASSISTANT_REQUEST_USE_WAKE_WORD:
      return ESPHOME_PSTR("VOICE_ASSISTANT_REQUEST_USE_WAKE_WORD");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#ifdef USE_VOICE_ASSISTANT
template<> const char *proto_enum_to_string<enums::VoiceAssistantEvent>(enums::VoiceAssistantEvent value) {
  switch (value) {
    case enums::VOICE_ASSISTANT_ERROR:
      return ESPHOME_PSTR("VOICE_ASSISTANT_ERROR");
    case enums::VOICE_ASSISTANT_RUN_START:
      return ESPHOME_PSTR("VOICE_ASSISTANT_RUN_START");
    case enums::VOICE_ASSISTANT_RUN_END:
      return ESPHOME_PSTR("VOICE_ASSISTANT_RUN_END");
    case enums::VOICE_ASSISTANT_STT_START:
      return ESPHOME_PSTR("VOICE_ASSISTANT_STT_START");
    case enums::VOICE_ASSISTANT_STT_END:
      return ESPHOME_PSTR("VOICE_ASSISTANT_STT_END");
    case enums::VOICE_ASSISTANT_INTENT_START:
      return ESPHOME_PSTR("VOICE_ASSISTANT_INTENT_START");
    case enums::VOICE_ASSISTANT_INTENT_END:
      return ESPHOME_PSTR("VOICE_ASSISTANT_INTENT_END");
    case enums::VOICE_ASSISTANT_TTS_START:
      return ESPHOME_PSTR("VOICE_ASSISTANT_TTS_START");
    case enums::VOICE_ASSISTANT_TTS_END:
      return ESPHOME_PSTR("VOICE_ASSISTANT_TTS_END");
    case enums::VOICE_ASSISTANT_WAKE_WORD_START:
      return ESPHOME_PSTR("VOICE_ASSISTANT_WAKE_WORD_START");
    case enums::VOICE_ASSISTANT_WAKE_WORD_END:
      return ESPHOME_PSTR("VOICE_ASSISTANT_WAKE_WORD_END");
    case enums::VOICE_ASSISTANT_STT_VAD_START:
      return ESPHOME_PSTR("VOICE_ASSISTANT_STT_VAD_START");
    case enums::VOICE_ASSISTANT_STT_VAD_END:
      return ESPHOME_PSTR("VOICE_ASSISTANT_STT_VAD_END");
    case enums::VOICE_ASSISTANT_TTS_STREAM_START:
      return ESPHOME_PSTR("VOICE_ASSISTANT_TTS_STREAM_START");
    case enums::VOICE_ASSISTANT_TTS_STREAM_END:
      return ESPHOME_PSTR("VOICE_ASSISTANT_TTS_STREAM_END");
    case enums::VOICE_ASSISTANT_INTENT_PROGRESS:
      return ESPHOME_PSTR("VOICE_ASSISTANT_INTENT_PROGRESS");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::VoiceAssistantTimerEvent>(enums::VoiceAssistantTimerEvent value) {
  switch (value) {
    case enums::VOICE_ASSISTANT_TIMER_STARTED:
      return ESPHOME_PSTR("VOICE_ASSISTANT_TIMER_STARTED");
    case enums::VOICE_ASSISTANT_TIMER_UPDATED:
      return ESPHOME_PSTR("VOICE_ASSISTANT_TIMER_UPDATED");
    case enums::VOICE_ASSISTANT_TIMER_CANCELLED:
      return ESPHOME_PSTR("VOICE_ASSISTANT_TIMER_CANCELLED");
    case enums::VOICE_ASSISTANT_TIMER_FINISHED:
      return ESPHOME_PSTR("VOICE_ASSISTANT_TIMER_FINISHED");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
template<> const char *proto_enum_to_string<enums::AlarmControlPanelState>(enums::AlarmControlPanelState value) {
  switch (value) {
    case enums::ALARM_STATE_DISARMED:
      return ESPHOME_PSTR("ALARM_STATE_DISARMED");
    case enums::ALARM_STATE_ARMED_HOME:
      return ESPHOME_PSTR("ALARM_STATE_ARMED_HOME");
    case enums::ALARM_STATE_ARMED_AWAY:
      return ESPHOME_PSTR("ALARM_STATE_ARMED_AWAY");
    case enums::ALARM_STATE_ARMED_NIGHT:
      return ESPHOME_PSTR("ALARM_STATE_ARMED_NIGHT");
    case enums::ALARM_STATE_ARMED_VACATION:
      return ESPHOME_PSTR("ALARM_STATE_ARMED_VACATION");
    case enums::ALARM_STATE_ARMED_CUSTOM_BYPASS:
      return ESPHOME_PSTR("ALARM_STATE_ARMED_CUSTOM_BYPASS");
    case enums::ALARM_STATE_PENDING:
      return ESPHOME_PSTR("ALARM_STATE_PENDING");
    case enums::ALARM_STATE_ARMING:
      return ESPHOME_PSTR("ALARM_STATE_ARMING");
    case enums::ALARM_STATE_DISARMING:
      return ESPHOME_PSTR("ALARM_STATE_DISARMING");
    case enums::ALARM_STATE_TRIGGERED:
      return ESPHOME_PSTR("ALARM_STATE_TRIGGERED");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<>
const char *proto_enum_to_string<enums::AlarmControlPanelStateCommand>(enums::AlarmControlPanelStateCommand value) {
  switch (value) {
    case enums::ALARM_CONTROL_PANEL_DISARM:
      return ESPHOME_PSTR("ALARM_CONTROL_PANEL_DISARM");
    case enums::ALARM_CONTROL_PANEL_ARM_AWAY:
      return ESPHOME_PSTR("ALARM_CONTROL_PANEL_ARM_AWAY");
    case enums::ALARM_CONTROL_PANEL_ARM_HOME:
      return ESPHOME_PSTR("ALARM_CONTROL_PANEL_ARM_HOME");
    case enums::ALARM_CONTROL_PANEL_ARM_NIGHT:
      return ESPHOME_PSTR("ALARM_CONTROL_PANEL_ARM_NIGHT");
    case enums::ALARM_CONTROL_PANEL_ARM_VACATION:
      return ESPHOME_PSTR("ALARM_CONTROL_PANEL_ARM_VACATION");
    case enums::ALARM_CONTROL_PANEL_ARM_CUSTOM_BYPASS:
      return ESPHOME_PSTR("ALARM_CONTROL_PANEL_ARM_CUSTOM_BYPASS");
    case enums::ALARM_CONTROL_PANEL_TRIGGER:
      return ESPHOME_PSTR("ALARM_CONTROL_PANEL_TRIGGER");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_TEXT
template<> const char *proto_enum_to_string<enums::TextMode>(enums::TextMode value) {
  switch (value) {
    case enums::TEXT_MODE_TEXT:
      return ESPHOME_PSTR("TEXT_MODE_TEXT");
    case enums::TEXT_MODE_PASSWORD:
      return ESPHOME_PSTR("TEXT_MODE_PASSWORD");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_VALVE
template<> const char *proto_enum_to_string<enums::ValveOperation>(enums::ValveOperation value) {
  switch (value) {
    case enums::VALVE_OPERATION_IDLE:
      return ESPHOME_PSTR("VALVE_OPERATION_IDLE");
    case enums::VALVE_OPERATION_IS_OPENING:
      return ESPHOME_PSTR("VALVE_OPERATION_IS_OPENING");
    case enums::VALVE_OPERATION_IS_CLOSING:
      return ESPHOME_PSTR("VALVE_OPERATION_IS_CLOSING");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_UPDATE
template<> const char *proto_enum_to_string<enums::UpdateCommand>(enums::UpdateCommand value) {
  switch (value) {
    case enums::UPDATE_COMMAND_NONE:
      return ESPHOME_PSTR("UPDATE_COMMAND_NONE");
    case enums::UPDATE_COMMAND_UPDATE:
      return ESPHOME_PSTR("UPDATE_COMMAND_UPDATE");
    case enums::UPDATE_COMMAND_CHECK:
      return ESPHOME_PSTR("UPDATE_COMMAND_CHECK");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_ZWAVE_PROXY
template<> const char *proto_enum_to_string<enums::ZWaveProxyRequestType>(enums::ZWaveProxyRequestType value) {
  switch (value) {
    case enums::ZWAVE_PROXY_REQUEST_TYPE_SUBSCRIBE:
      return ESPHOME_PSTR("ZWAVE_PROXY_REQUEST_TYPE_SUBSCRIBE");
    case enums::ZWAVE_PROXY_REQUEST_TYPE_UNSUBSCRIBE:
      return ESPHOME_PSTR("ZWAVE_PROXY_REQUEST_TYPE_UNSUBSCRIBE");
    case enums::ZWAVE_PROXY_REQUEST_TYPE_HOME_ID_CHANGE:
      return ESPHOME_PSTR("ZWAVE_PROXY_REQUEST_TYPE_HOME_ID_CHANGE");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif
#ifdef USE_SERIAL_PROXY
template<> const char *proto_enum_to_string<enums::SerialProxyParity>(enums::SerialProxyParity value) {
  switch (value) {
    case enums::SERIAL_PROXY_PARITY_NONE:
      return ESPHOME_PSTR("SERIAL_PROXY_PARITY_NONE");
    case enums::SERIAL_PROXY_PARITY_EVEN:
      return ESPHOME_PSTR("SERIAL_PROXY_PARITY_EVEN");
    case enums::SERIAL_PROXY_PARITY_ODD:
      return ESPHOME_PSTR("SERIAL_PROXY_PARITY_ODD");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::SerialProxyRequestType>(enums::SerialProxyRequestType value) {
  switch (value) {
    case enums::SERIAL_PROXY_REQUEST_TYPE_SUBSCRIBE:
      return ESPHOME_PSTR("SERIAL_PROXY_REQUEST_TYPE_SUBSCRIBE");
    case enums::SERIAL_PROXY_REQUEST_TYPE_UNSUBSCRIBE:
      return ESPHOME_PSTR("SERIAL_PROXY_REQUEST_TYPE_UNSUBSCRIBE");
    case enums::SERIAL_PROXY_REQUEST_TYPE_FLUSH:
      return ESPHOME_PSTR("SERIAL_PROXY_REQUEST_TYPE_FLUSH");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
template<> const char *proto_enum_to_string<enums::SerialProxyStatus>(enums::SerialProxyStatus value) {
  switch (value) {
    case enums::SERIAL_PROXY_STATUS_OK:
      return ESPHOME_PSTR("SERIAL_PROXY_STATUS_OK");
    case enums::SERIAL_PROXY_STATUS_ASSUMED_SUCCESS:
      return ESPHOME_PSTR("SERIAL_PROXY_STATUS_ASSUMED_SUCCESS");
    case enums::SERIAL_PROXY_STATUS_ERROR:
      return ESPHOME_PSTR("SERIAL_PROXY_STATUS_ERROR");
    case enums::SERIAL_PROXY_STATUS_TIMEOUT:
      return ESPHOME_PSTR("SERIAL_PROXY_STATUS_TIMEOUT");
    case enums::SERIAL_PROXY_STATUS_NOT_SUPPORTED:
      return ESPHOME_PSTR("SERIAL_PROXY_STATUS_NOT_SUPPORTED");
    default:
      return ESPHOME_PSTR("UNKNOWN");
  }
}
#endif

const char *HelloRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("HelloRequest"));
  dump_field(out, ESPHOME_PSTR("client_info"), this->client_info);
  dump_field(out, ESPHOME_PSTR("api_version_major"), this->api_version_major);
  dump_field(out, ESPHOME_PSTR("api_version_minor"), this->api_version_minor);
  return out.c_str();
}
const char *HelloResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("HelloResponse"));
  dump_field(out, ESPHOME_PSTR("api_version_major"), this->api_version_major);
  dump_field(out, ESPHOME_PSTR("api_version_minor"), this->api_version_minor);
  dump_field(out, ESPHOME_PSTR("server_info"), this->server_info);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  return out.c_str();
}
const char *DisconnectRequest::dump_to(DumpBuffer &out) const {
  out.append_p(ESPHOME_PSTR("DisconnectRequest {}"));
  return out.c_str();
}
const char *DisconnectResponse::dump_to(DumpBuffer &out) const {
  out.append_p(ESPHOME_PSTR("DisconnectResponse {}"));
  return out.c_str();
}
const char *PingRequest::dump_to(DumpBuffer &out) const {
  out.append_p(ESPHOME_PSTR("PingRequest {}"));
  return out.c_str();
}
const char *PingResponse::dump_to(DumpBuffer &out) const {
  out.append_p(ESPHOME_PSTR("PingResponse {}"));
  return out.c_str();
}
#ifdef USE_AREAS
const char *AreaInfo::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("AreaInfo"));
  dump_field(out, ESPHOME_PSTR("area_id"), this->area_id);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  return out.c_str();
}
#endif
#ifdef USE_DEVICES
const char *DeviceInfo::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("DeviceInfo"));
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("area_id"), this->area_id);
  return out.c_str();
}
#endif
#ifdef USE_SERIAL_PROXY
const char *SerialProxyInfo::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SerialProxyInfo"));
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("port_type"), static_cast<enums::SerialProxyPortType>(this->port_type));
  return out.c_str();
}
#endif
const char *DeviceInfoResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("DeviceInfoResponse"));
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("mac_address"), this->mac_address);
  dump_field(out, ESPHOME_PSTR("esphome_version"), this->esphome_version);
  dump_field(out, ESPHOME_PSTR("compilation_time"), this->compilation_time);
  dump_field(out, ESPHOME_PSTR("model"), this->model);
#ifdef USE_DEEP_SLEEP
  dump_field(out, ESPHOME_PSTR("has_deep_sleep"), this->has_deep_sleep);
#endif
#ifdef ESPHOME_PROJECT_NAME
  dump_field(out, ESPHOME_PSTR("project_name"), this->project_name);
#endif
#ifdef ESPHOME_PROJECT_NAME
  dump_field(out, ESPHOME_PSTR("project_version"), this->project_version);
#endif
#ifdef USE_WEBSERVER
  dump_field(out, ESPHOME_PSTR("webserver_port"), this->webserver_port);
#endif
#ifdef USE_BLUETOOTH_PROXY
  dump_field(out, ESPHOME_PSTR("bluetooth_proxy_feature_flags"), this->bluetooth_proxy_feature_flags);
#endif
  dump_field(out, ESPHOME_PSTR("manufacturer"), this->manufacturer);
  dump_field(out, ESPHOME_PSTR("friendly_name"), this->friendly_name);
#ifdef USE_VOICE_ASSISTANT
  dump_field(out, ESPHOME_PSTR("voice_assistant_feature_flags"), this->voice_assistant_feature_flags);
#endif
#ifdef USE_AREAS
  dump_field(out, ESPHOME_PSTR("suggested_area"), this->suggested_area);
#endif
#ifdef USE_BLUETOOTH_PROXY
  dump_field(out, ESPHOME_PSTR("bluetooth_mac_address"), this->bluetooth_mac_address);
#endif
#ifdef USE_API_NOISE
  dump_field(out, ESPHOME_PSTR("api_encryption_supported"), this->api_encryption_supported);
#endif
#ifdef USE_DEVICES
  for (const auto &it : this->devices) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("devices")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
#endif
#ifdef USE_AREAS
  for (const auto &it : this->areas) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("areas")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
#endif
#ifdef USE_AREAS
  out.append(2, ' ').append_p(ESPHOME_PSTR("area")).append(": ");
  this->area.dump_to(out);
  out.append("\n");
#endif
#ifdef USE_ZWAVE_PROXY
  dump_field(out, ESPHOME_PSTR("zwave_proxy_feature_flags"), this->zwave_proxy_feature_flags);
#endif
#ifdef USE_ZWAVE_PROXY
  dump_field(out, ESPHOME_PSTR("zwave_home_id"), this->zwave_home_id);
#endif
#ifdef USE_SERIAL_PROXY
  for (const auto &it : this->serial_proxies) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("serial_proxies")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
#endif
  return out.c_str();
}
const char *ListEntitiesDoneResponse::dump_to(DumpBuffer &out) const {
  out.append_p(ESPHOME_PSTR("ListEntitiesDoneResponse {}"));
  return out.c_str();
}
#ifdef USE_BINARY_SENSOR
const char *ListEntitiesBinarySensorResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesBinarySensorResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("device_class"), this->device_class);
  dump_field(out, ESPHOME_PSTR("is_status_binary_sensor"), this->is_status_binary_sensor);
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *BinarySensorStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BinarySensorStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("missing_state"), this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_COVER
const char *ListEntitiesCoverResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesCoverResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("assumed_state"), this->assumed_state);
  dump_field(out, ESPHOME_PSTR("supports_position"), this->supports_position);
  dump_field(out, ESPHOME_PSTR("supports_tilt"), this->supports_tilt);
  dump_field(out, ESPHOME_PSTR("device_class"), this->device_class);
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("supports_stop"), this->supports_stop);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *CoverStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("CoverStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("position"), this->position);
  dump_field(out, ESPHOME_PSTR("tilt"), this->tilt);
  dump_field(out, ESPHOME_PSTR("current_operation"), static_cast<enums::CoverOperation>(this->current_operation));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *CoverCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("CoverCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("has_position"), this->has_position);
  dump_field(out, ESPHOME_PSTR("position"), this->position);
  dump_field(out, ESPHOME_PSTR("has_tilt"), this->has_tilt);
  dump_field(out, ESPHOME_PSTR("tilt"), this->tilt);
  dump_field(out, ESPHOME_PSTR("stop"), this->stop);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_FAN
const char *ListEntitiesFanResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesFanResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("supports_oscillation"), this->supports_oscillation);
  dump_field(out, ESPHOME_PSTR("supports_speed"), this->supports_speed);
  dump_field(out, ESPHOME_PSTR("supports_direction"), this->supports_direction);
  dump_field(out, ESPHOME_PSTR("supported_speed_count"), this->supported_speed_count);
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  for (const auto &it : *this->supported_preset_modes) {
    dump_field(out, ESPHOME_PSTR("supported_preset_modes"), it, 4);
  }
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *FanStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("FanStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("oscillating"), this->oscillating);
  dump_field(out, ESPHOME_PSTR("direction"), static_cast<enums::FanDirection>(this->direction));
  dump_field(out, ESPHOME_PSTR("speed_level"), this->speed_level);
  dump_field(out, ESPHOME_PSTR("preset_mode"), this->preset_mode);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *FanCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("FanCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("has_state"), this->has_state);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("has_oscillating"), this->has_oscillating);
  dump_field(out, ESPHOME_PSTR("oscillating"), this->oscillating);
  dump_field(out, ESPHOME_PSTR("has_direction"), this->has_direction);
  dump_field(out, ESPHOME_PSTR("direction"), static_cast<enums::FanDirection>(this->direction));
  dump_field(out, ESPHOME_PSTR("has_speed_level"), this->has_speed_level);
  dump_field(out, ESPHOME_PSTR("speed_level"), this->speed_level);
  dump_field(out, ESPHOME_PSTR("has_preset_mode"), this->has_preset_mode);
  dump_field(out, ESPHOME_PSTR("preset_mode"), this->preset_mode);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_LIGHT
const char *ListEntitiesLightResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesLightResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  for (const auto &it : *this->supported_color_modes) {
    dump_field(out, ESPHOME_PSTR("supported_color_modes"), static_cast<enums::ColorMode>(it), 4);
  }
  dump_field(out, ESPHOME_PSTR("min_mireds"), this->min_mireds);
  dump_field(out, ESPHOME_PSTR("max_mireds"), this->max_mireds);
  for (const auto &it : *this->effects) {
    dump_field(out, ESPHOME_PSTR("effects"), it, 4);
  }
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *LightStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("LightStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("brightness"), this->brightness);
  dump_field(out, ESPHOME_PSTR("color_mode"), static_cast<enums::ColorMode>(this->color_mode));
  dump_field(out, ESPHOME_PSTR("color_brightness"), this->color_brightness);
  dump_field(out, ESPHOME_PSTR("red"), this->red);
  dump_field(out, ESPHOME_PSTR("green"), this->green);
  dump_field(out, ESPHOME_PSTR("blue"), this->blue);
  dump_field(out, ESPHOME_PSTR("white"), this->white);
  dump_field(out, ESPHOME_PSTR("color_temperature"), this->color_temperature);
  dump_field(out, ESPHOME_PSTR("cold_white"), this->cold_white);
  dump_field(out, ESPHOME_PSTR("warm_white"), this->warm_white);
  dump_field(out, ESPHOME_PSTR("effect"), this->effect);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *LightCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("LightCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("has_state"), this->has_state);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("has_brightness"), this->has_brightness);
  dump_field(out, ESPHOME_PSTR("brightness"), this->brightness);
  dump_field(out, ESPHOME_PSTR("has_color_mode"), this->has_color_mode);
  dump_field(out, ESPHOME_PSTR("color_mode"), static_cast<enums::ColorMode>(this->color_mode));
  dump_field(out, ESPHOME_PSTR("has_color_brightness"), this->has_color_brightness);
  dump_field(out, ESPHOME_PSTR("color_brightness"), this->color_brightness);
  dump_field(out, ESPHOME_PSTR("has_rgb"), this->has_rgb);
  dump_field(out, ESPHOME_PSTR("red"), this->red);
  dump_field(out, ESPHOME_PSTR("green"), this->green);
  dump_field(out, ESPHOME_PSTR("blue"), this->blue);
  dump_field(out, ESPHOME_PSTR("has_white"), this->has_white);
  dump_field(out, ESPHOME_PSTR("white"), this->white);
  dump_field(out, ESPHOME_PSTR("has_color_temperature"), this->has_color_temperature);
  dump_field(out, ESPHOME_PSTR("color_temperature"), this->color_temperature);
  dump_field(out, ESPHOME_PSTR("has_cold_white"), this->has_cold_white);
  dump_field(out, ESPHOME_PSTR("cold_white"), this->cold_white);
  dump_field(out, ESPHOME_PSTR("has_warm_white"), this->has_warm_white);
  dump_field(out, ESPHOME_PSTR("warm_white"), this->warm_white);
  dump_field(out, ESPHOME_PSTR("has_transition_length"), this->has_transition_length);
  dump_field(out, ESPHOME_PSTR("transition_length"), this->transition_length);
  dump_field(out, ESPHOME_PSTR("has_flash_length"), this->has_flash_length);
  dump_field(out, ESPHOME_PSTR("flash_length"), this->flash_length);
  dump_field(out, ESPHOME_PSTR("has_effect"), this->has_effect);
  dump_field(out, ESPHOME_PSTR("effect"), this->effect);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_SENSOR
const char *ListEntitiesSensorResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesSensorResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("unit_of_measurement"), this->unit_of_measurement);
  dump_field(out, ESPHOME_PSTR("accuracy_decimals"), this->accuracy_decimals);
  dump_field(out, ESPHOME_PSTR("force_update"), this->force_update);
  dump_field(out, ESPHOME_PSTR("device_class"), this->device_class);
  dump_field(out, ESPHOME_PSTR("state_class"), static_cast<enums::SensorStateClass>(this->state_class));
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *SensorStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SensorStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("missing_state"), this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_SWITCH
const char *ListEntitiesSwitchResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesSwitchResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("assumed_state"), this->assumed_state);
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("device_class"), this->device_class);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *SwitchStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SwitchStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *SwitchCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SwitchCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_TEXT_SENSOR
const char *ListEntitiesTextSensorResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesTextSensorResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("device_class"), this->device_class);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *TextSensorStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("TextSensorStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("missing_state"), this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
const char *SubscribeLogsRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SubscribeLogsRequest"));
  dump_field(out, ESPHOME_PSTR("level"), static_cast<enums::LogLevel>(this->level));
  dump_field(out, ESPHOME_PSTR("dump_config"), this->dump_config);
  return out.c_str();
}
const char *SubscribeLogsResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SubscribeLogsResponse"));
  dump_field(out, ESPHOME_PSTR("level"), static_cast<enums::LogLevel>(this->level));
  dump_bytes_field(out, ESPHOME_PSTR("message"), this->message_ptr_, this->message_len_);
  return out.c_str();
}
#ifdef USE_API_NOISE
const char *NoiseEncryptionSetKeyRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("NoiseEncryptionSetKeyRequest"));
  dump_bytes_field(out, ESPHOME_PSTR("key"), this->key, this->key_len);
  return out.c_str();
}
const char *NoiseEncryptionSetKeyResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("NoiseEncryptionSetKeyResponse"));
  dump_field(out, ESPHOME_PSTR("success"), this->success);
  return out.c_str();
}
#endif
#ifdef USE_API_HOMEASSISTANT_SERVICES
const char *HomeassistantServiceMap::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("HomeassistantServiceMap"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("value"), this->value);
  return out.c_str();
}
const char *HomeassistantActionRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("HomeassistantActionRequest"));
  dump_field(out, ESPHOME_PSTR("service"), this->service);
  for (const auto &it : this->data) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("data")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
  for (const auto &it : this->data_template) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("data_template")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
  for (const auto &it : this->variables) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("variables")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
  dump_field(out, ESPHOME_PSTR("is_event"), this->is_event);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
  dump_field(out, ESPHOME_PSTR("call_id"), this->call_id);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  dump_field(out, ESPHOME_PSTR("wants_response"), this->wants_response);
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  dump_field(out, ESPHOME_PSTR("response_template"), this->response_template);
#endif
  return out.c_str();
}
#endif
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES
const char *HomeassistantActionResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("HomeassistantActionResponse"));
  dump_field(out, ESPHOME_PSTR("call_id"), this->call_id);
  dump_field(out, ESPHOME_PSTR("success"), this->success);
  dump_field(out, ESPHOME_PSTR("error_message"), this->error_message);
#ifdef USE_API_HOMEASSISTANT_ACTION_RESPONSES_JSON
  dump_bytes_field(out, ESPHOME_PSTR("response_data"), this->response_data, this->response_data_len);
#endif
  return out.c_str();
}
#endif
#ifdef USE_API_HOMEASSISTANT_STATES
const char *SubscribeHomeAssistantStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SubscribeHomeAssistantStateResponse"));
  dump_field(out, ESPHOME_PSTR("entity_id"), this->entity_id);
  dump_field(out, ESPHOME_PSTR("attribute"), this->attribute);
  dump_field(out, ESPHOME_PSTR("once"), this->once);
  return out.c_str();
}
const char *HomeAssistantStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("HomeAssistantStateResponse"));
  dump_field(out, ESPHOME_PSTR("entity_id"), this->entity_id);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("attribute"), this->attribute);
  return out.c_str();
}
#endif
const char *GetTimeRequest::dump_to(DumpBuffer &out) const {
  out.append_p(ESPHOME_PSTR("GetTimeRequest {}"));
  return out.c_str();
}
const char *DSTRule::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("DSTRule"));
  dump_field(out, ESPHOME_PSTR("time_seconds"), this->time_seconds);
  dump_field(out, ESPHOME_PSTR("day"), this->day);
  dump_field(out, ESPHOME_PSTR("type"), static_cast<enums::DSTRuleType>(this->type));
  dump_field(out, ESPHOME_PSTR("month"), this->month);
  dump_field(out, ESPHOME_PSTR("week"), this->week);
  dump_field(out, ESPHOME_PSTR("day_of_week"), this->day_of_week);
  return out.c_str();
}
const char *ParsedTimezone::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ParsedTimezone"));
  dump_field(out, ESPHOME_PSTR("std_offset_seconds"), this->std_offset_seconds);
  dump_field(out, ESPHOME_PSTR("dst_offset_seconds"), this->dst_offset_seconds);
  out.append(2, ' ').append_p(ESPHOME_PSTR("dst_start")).append(": ");
  this->dst_start.dump_to(out);
  out.append("\n");
  out.append(2, ' ').append_p(ESPHOME_PSTR("dst_end")).append(": ");
  this->dst_end.dump_to(out);
  out.append("\n");
  return out.c_str();
}
const char *GetTimeResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("GetTimeResponse"));
  dump_field(out, ESPHOME_PSTR("epoch_seconds"), this->epoch_seconds);
  dump_field(out, ESPHOME_PSTR("timezone"), this->timezone);
  out.append(2, ' ').append_p(ESPHOME_PSTR("parsed_timezone")).append(": ");
  this->parsed_timezone.dump_to(out);
  out.append("\n");
  return out.c_str();
}
#ifdef USE_API_USER_DEFINED_ACTIONS
const char *ListEntitiesServicesArgument::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesServicesArgument"));
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("type"), static_cast<enums::ServiceArgType>(this->type));
  return out.c_str();
}
const char *ListEntitiesServicesResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesServicesResponse"));
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  for (const auto &it : this->args) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("args")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
  dump_field(out, ESPHOME_PSTR("supports_response"), static_cast<enums::SupportsResponseType>(this->supports_response));
  return out.c_str();
}
const char *ExecuteServiceArgument::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ExecuteServiceArgument"));
  dump_field(out, ESPHOME_PSTR("bool_"), this->bool_);
  dump_field(out, ESPHOME_PSTR("legacy_int"), this->legacy_int);
  dump_field(out, ESPHOME_PSTR("float_"), this->float_);
  dump_field(out, ESPHOME_PSTR("string_"), this->string_);
  dump_field(out, ESPHOME_PSTR("int_"), this->int_);
  for (const auto it : this->bool_array) {
    dump_field(out, ESPHOME_PSTR("bool_array"), static_cast<bool>(it), 4);
  }
  for (const auto &it : this->int_array) {
    dump_field(out, ESPHOME_PSTR("int_array"), it, 4);
  }
  for (const auto &it : this->float_array) {
    dump_field(out, ESPHOME_PSTR("float_array"), it, 4);
  }
  for (const auto &it : this->string_array) {
    dump_field(out, ESPHOME_PSTR("string_array"), it, 4);
  }
  return out.c_str();
}
const char *ExecuteServiceRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ExecuteServiceRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  for (const auto &it : this->args) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("args")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  dump_field(out, ESPHOME_PSTR("call_id"), this->call_id);
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
  dump_field(out, ESPHOME_PSTR("return_response"), this->return_response);
#endif
  return out.c_str();
}
#endif
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES
const char *ExecuteServiceResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ExecuteServiceResponse"));
  dump_field(out, ESPHOME_PSTR("call_id"), this->call_id);
  dump_field(out, ESPHOME_PSTR("success"), this->success);
  dump_field(out, ESPHOME_PSTR("error_message"), this->error_message);
#ifdef USE_API_USER_DEFINED_ACTION_RESPONSES_JSON
  dump_bytes_field(out, ESPHOME_PSTR("response_data"), this->response_data, this->response_data_len);
#endif
  return out.c_str();
}
#endif
#ifdef USE_CAMERA
const char *ListEntitiesCameraResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesCameraResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *CameraImageResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("CameraImageResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_bytes_field(out, ESPHOME_PSTR("data"), this->data_ptr_, this->data_len_);
  dump_field(out, ESPHOME_PSTR("done"), this->done);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *CameraImageRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("CameraImageRequest"));
  dump_field(out, ESPHOME_PSTR("single"), this->single);
  dump_field(out, ESPHOME_PSTR("stream"), this->stream);
  return out.c_str();
}
#endif
#ifdef USE_CLIMATE
const char *ListEntitiesClimateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesClimateResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("supports_current_temperature"), this->supports_current_temperature);
  dump_field(out, ESPHOME_PSTR("supports_two_point_target_temperature"), this->supports_two_point_target_temperature);
  for (const auto &it : *this->supported_modes) {
    dump_field(out, ESPHOME_PSTR("supported_modes"), static_cast<enums::ClimateMode>(it), 4);
  }
  dump_field(out, ESPHOME_PSTR("visual_min_temperature"), this->visual_min_temperature);
  dump_field(out, ESPHOME_PSTR("visual_max_temperature"), this->visual_max_temperature);
  dump_field(out, ESPHOME_PSTR("visual_target_temperature_step"), this->visual_target_temperature_step);
  dump_field(out, ESPHOME_PSTR("supports_action"), this->supports_action);
  for (const auto &it : *this->supported_fan_modes) {
    dump_field(out, ESPHOME_PSTR("supported_fan_modes"), static_cast<enums::ClimateFanMode>(it), 4);
  }
  for (const auto &it : *this->supported_swing_modes) {
    dump_field(out, ESPHOME_PSTR("supported_swing_modes"), static_cast<enums::ClimateSwingMode>(it), 4);
  }
  for (const auto &it : *this->supported_custom_fan_modes) {
    dump_field(out, ESPHOME_PSTR("supported_custom_fan_modes"), it, 4);
  }
  for (const auto &it : *this->supported_presets) {
    dump_field(out, ESPHOME_PSTR("supported_presets"), static_cast<enums::ClimatePreset>(it), 4);
  }
  for (const auto &it : *this->supported_custom_presets) {
    dump_field(out, ESPHOME_PSTR("supported_custom_presets"), it, 4);
  }
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("visual_current_temperature_step"), this->visual_current_temperature_step);
  dump_field(out, ESPHOME_PSTR("supports_current_humidity"), this->supports_current_humidity);
  dump_field(out, ESPHOME_PSTR("supports_target_humidity"), this->supports_target_humidity);
  dump_field(out, ESPHOME_PSTR("visual_min_humidity"), this->visual_min_humidity);
  dump_field(out, ESPHOME_PSTR("visual_max_humidity"), this->visual_max_humidity);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  dump_field(out, ESPHOME_PSTR("feature_flags"), this->feature_flags);
  dump_field(out, ESPHOME_PSTR("temperature_unit"), static_cast<enums::TemperatureUnit>(this->temperature_unit));
  return out.c_str();
}
const char *ClimateStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ClimateStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("mode"), static_cast<enums::ClimateMode>(this->mode));
  dump_field(out, ESPHOME_PSTR("current_temperature"), this->current_temperature);
  dump_field(out, ESPHOME_PSTR("target_temperature"), this->target_temperature);
  dump_field(out, ESPHOME_PSTR("target_temperature_low"), this->target_temperature_low);
  dump_field(out, ESPHOME_PSTR("target_temperature_high"), this->target_temperature_high);
  dump_field(out, ESPHOME_PSTR("action"), static_cast<enums::ClimateAction>(this->action));
  dump_field(out, ESPHOME_PSTR("fan_mode"), static_cast<enums::ClimateFanMode>(this->fan_mode));
  dump_field(out, ESPHOME_PSTR("swing_mode"), static_cast<enums::ClimateSwingMode>(this->swing_mode));
  dump_field(out, ESPHOME_PSTR("custom_fan_mode"), this->custom_fan_mode);
  dump_field(out, ESPHOME_PSTR("preset"), static_cast<enums::ClimatePreset>(this->preset));
  dump_field(out, ESPHOME_PSTR("custom_preset"), this->custom_preset);
  dump_field(out, ESPHOME_PSTR("current_humidity"), this->current_humidity);
  dump_field(out, ESPHOME_PSTR("target_humidity"), this->target_humidity);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *ClimateCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ClimateCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("has_mode"), this->has_mode);
  dump_field(out, ESPHOME_PSTR("mode"), static_cast<enums::ClimateMode>(this->mode));
  dump_field(out, ESPHOME_PSTR("has_target_temperature"), this->has_target_temperature);
  dump_field(out, ESPHOME_PSTR("target_temperature"), this->target_temperature);
  dump_field(out, ESPHOME_PSTR("has_target_temperature_low"), this->has_target_temperature_low);
  dump_field(out, ESPHOME_PSTR("target_temperature_low"), this->target_temperature_low);
  dump_field(out, ESPHOME_PSTR("has_target_temperature_high"), this->has_target_temperature_high);
  dump_field(out, ESPHOME_PSTR("target_temperature_high"), this->target_temperature_high);
  dump_field(out, ESPHOME_PSTR("has_fan_mode"), this->has_fan_mode);
  dump_field(out, ESPHOME_PSTR("fan_mode"), static_cast<enums::ClimateFanMode>(this->fan_mode));
  dump_field(out, ESPHOME_PSTR("has_swing_mode"), this->has_swing_mode);
  dump_field(out, ESPHOME_PSTR("swing_mode"), static_cast<enums::ClimateSwingMode>(this->swing_mode));
  dump_field(out, ESPHOME_PSTR("has_custom_fan_mode"), this->has_custom_fan_mode);
  dump_field(out, ESPHOME_PSTR("custom_fan_mode"), this->custom_fan_mode);
  dump_field(out, ESPHOME_PSTR("has_preset"), this->has_preset);
  dump_field(out, ESPHOME_PSTR("preset"), static_cast<enums::ClimatePreset>(this->preset));
  dump_field(out, ESPHOME_PSTR("has_custom_preset"), this->has_custom_preset);
  dump_field(out, ESPHOME_PSTR("custom_preset"), this->custom_preset);
  dump_field(out, ESPHOME_PSTR("has_target_humidity"), this->has_target_humidity);
  dump_field(out, ESPHOME_PSTR("target_humidity"), this->target_humidity);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_WATER_HEATER
const char *ListEntitiesWaterHeaterResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesWaterHeaterResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  dump_field(out, ESPHOME_PSTR("min_temperature"), this->min_temperature);
  dump_field(out, ESPHOME_PSTR("max_temperature"), this->max_temperature);
  dump_field(out, ESPHOME_PSTR("target_temperature_step"), this->target_temperature_step);
  for (const auto &it : *this->supported_modes) {
    dump_field(out, ESPHOME_PSTR("supported_modes"), static_cast<enums::WaterHeaterMode>(it), 4);
  }
  dump_field(out, ESPHOME_PSTR("supported_features"), this->supported_features);
  dump_field(out, ESPHOME_PSTR("temperature_unit"), static_cast<enums::TemperatureUnit>(this->temperature_unit));
  return out.c_str();
}
const char *WaterHeaterStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("WaterHeaterStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("current_temperature"), this->current_temperature);
  dump_field(out, ESPHOME_PSTR("target_temperature"), this->target_temperature);
  dump_field(out, ESPHOME_PSTR("mode"), static_cast<enums::WaterHeaterMode>(this->mode));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("target_temperature_low"), this->target_temperature_low);
  dump_field(out, ESPHOME_PSTR("target_temperature_high"), this->target_temperature_high);
  return out.c_str();
}
const char *WaterHeaterCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("WaterHeaterCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("has_fields"), this->has_fields);
  dump_field(out, ESPHOME_PSTR("mode"), static_cast<enums::WaterHeaterMode>(this->mode));
  dump_field(out, ESPHOME_PSTR("target_temperature"), this->target_temperature);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("target_temperature_low"), this->target_temperature_low);
  dump_field(out, ESPHOME_PSTR("target_temperature_high"), this->target_temperature_high);
  return out.c_str();
}
#endif
#ifdef USE_NUMBER
const char *ListEntitiesNumberResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesNumberResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("min_value"), this->min_value);
  dump_field(out, ESPHOME_PSTR("max_value"), this->max_value);
  dump_field(out, ESPHOME_PSTR("step"), this->step);
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("unit_of_measurement"), this->unit_of_measurement);
  dump_field(out, ESPHOME_PSTR("mode"), static_cast<enums::NumberMode>(this->mode));
  dump_field(out, ESPHOME_PSTR("device_class"), this->device_class);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *NumberStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("NumberStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("missing_state"), this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *NumberCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("NumberCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_SELECT
const char *ListEntitiesSelectResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesSelectResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  for (const auto &it : *this->options) {
    dump_field(out, ESPHOME_PSTR("options"), it, 4);
  }
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *SelectStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SelectStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("missing_state"), this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *SelectCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SelectCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_SIREN
const char *ListEntitiesSirenResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesSirenResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  for (const auto &it : *this->tones) {
    dump_field(out, ESPHOME_PSTR("tones"), it, 4);
  }
  dump_field(out, ESPHOME_PSTR("supports_duration"), this->supports_duration);
  dump_field(out, ESPHOME_PSTR("supports_volume"), this->supports_volume);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *SirenStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SirenStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *SirenCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SirenCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("has_state"), this->has_state);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("has_tone"), this->has_tone);
  dump_field(out, ESPHOME_PSTR("tone"), this->tone);
  dump_field(out, ESPHOME_PSTR("has_duration"), this->has_duration);
  dump_field(out, ESPHOME_PSTR("duration"), this->duration);
  dump_field(out, ESPHOME_PSTR("has_volume"), this->has_volume);
  dump_field(out, ESPHOME_PSTR("volume"), this->volume);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_LOCK
const char *ListEntitiesLockResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesLockResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("assumed_state"), this->assumed_state);
  dump_field(out, ESPHOME_PSTR("supports_open"), this->supports_open);
  dump_field(out, ESPHOME_PSTR("requires_code"), this->requires_code);
  dump_field(out, ESPHOME_PSTR("code_format"), this->code_format);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *LockStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("LockStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), static_cast<enums::LockState>(this->state));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *LockCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("LockCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("command"), static_cast<enums::LockCommand>(this->command));
  dump_field(out, ESPHOME_PSTR("has_code"), this->has_code);
  dump_field(out, ESPHOME_PSTR("code"), this->code);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_BUTTON
const char *ListEntitiesButtonResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesButtonResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("device_class"), this->device_class);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *ButtonCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ButtonCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_MEDIA_PLAYER
const char *MediaPlayerSupportedFormat::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("MediaPlayerSupportedFormat"));
  dump_field(out, ESPHOME_PSTR("format"), this->format);
  dump_field(out, ESPHOME_PSTR("sample_rate"), this->sample_rate);
  dump_field(out, ESPHOME_PSTR("num_channels"), this->num_channels);
  dump_field(out, ESPHOME_PSTR("purpose"), static_cast<enums::MediaPlayerFormatPurpose>(this->purpose));
  dump_field(out, ESPHOME_PSTR("sample_bytes"), this->sample_bytes);
  return out.c_str();
}
const char *ListEntitiesMediaPlayerResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesMediaPlayerResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("supports_pause"), this->supports_pause);
  for (const auto &it : this->supported_formats) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("supported_formats")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  dump_field(out, ESPHOME_PSTR("feature_flags"), this->feature_flags);
  return out.c_str();
}
const char *MediaPlayerStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("MediaPlayerStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), static_cast<enums::MediaPlayerState>(this->state));
  dump_field(out, ESPHOME_PSTR("volume"), this->volume);
  dump_field(out, ESPHOME_PSTR("muted"), this->muted);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *MediaPlayerCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("MediaPlayerCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("has_command"), this->has_command);
  dump_field(out, ESPHOME_PSTR("command"), static_cast<enums::MediaPlayerCommand>(this->command));
  dump_field(out, ESPHOME_PSTR("has_volume"), this->has_volume);
  dump_field(out, ESPHOME_PSTR("volume"), this->volume);
  dump_field(out, ESPHOME_PSTR("has_media_url"), this->has_media_url);
  dump_field(out, ESPHOME_PSTR("media_url"), this->media_url);
  dump_field(out, ESPHOME_PSTR("has_announcement"), this->has_announcement);
  dump_field(out, ESPHOME_PSTR("announcement"), this->announcement);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_BLUETOOTH_PROXY
const char *SubscribeBluetoothLEAdvertisementsRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SubscribeBluetoothLEAdvertisementsRequest"));
  dump_field(out, ESPHOME_PSTR("flags"), this->flags);
  return out.c_str();
}
const char *BluetoothLERawAdvertisement::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothLERawAdvertisement"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("rssi"), this->rssi);
  dump_field(out, ESPHOME_PSTR("address_type"), this->address_type);
  dump_bytes_field(out, ESPHOME_PSTR("data"), this->data, this->data_len);
  return out.c_str();
}
const char *BluetoothLERawAdvertisementsResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothLERawAdvertisementsResponse"));
  for (uint16_t i = 0; i < this->advertisements_len; i++) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("advertisements")).append(": ");
    this->advertisements[i].dump_to(out);
    out.append("\n");
  }
  return out.c_str();
}
const char *BluetoothDeviceRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothDeviceRequest"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("request_type"), static_cast<enums::BluetoothDeviceRequestType>(this->request_type));
  dump_field(out, ESPHOME_PSTR("has_address_type"), this->has_address_type);
  dump_field(out, ESPHOME_PSTR("address_type"), this->address_type);
  return out.c_str();
}
const char *BluetoothDeviceConnectionResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothDeviceConnectionResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("connected"), this->connected);
  dump_field(out, ESPHOME_PSTR("mtu"), this->mtu);
  dump_field(out, ESPHOME_PSTR("error"), this->error);
  return out.c_str();
}
const char *BluetoothGATTGetServicesRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTGetServicesRequest"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  return out.c_str();
}
const char *BluetoothGATTDescriptor::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTDescriptor"));
  for (const auto &it : this->uuid) {
    dump_field(out, ESPHOME_PSTR("uuid"), it, 4);
  }
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  dump_field(out, ESPHOME_PSTR("short_uuid"), this->short_uuid);
  return out.c_str();
}
const char *BluetoothGATTCharacteristic::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTCharacteristic"));
  for (const auto &it : this->uuid) {
    dump_field(out, ESPHOME_PSTR("uuid"), it, 4);
  }
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  dump_field(out, ESPHOME_PSTR("properties"), this->properties);
  for (const auto &it : this->descriptors) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("descriptors")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
  dump_field(out, ESPHOME_PSTR("short_uuid"), this->short_uuid);
  return out.c_str();
}
const char *BluetoothGATTService::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTService"));
  for (const auto &it : this->uuid) {
    dump_field(out, ESPHOME_PSTR("uuid"), it, 4);
  }
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  for (const auto &it : this->characteristics) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("characteristics")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
  dump_field(out, ESPHOME_PSTR("short_uuid"), this->short_uuid);
  return out.c_str();
}
const char *BluetoothGATTGetServicesResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTGetServicesResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  for (const auto &it : this->services) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("services")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
  return out.c_str();
}
const char *BluetoothGATTGetServicesDoneResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTGetServicesDoneResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  return out.c_str();
}
const char *BluetoothGATTReadRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTReadRequest"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  return out.c_str();
}
const char *BluetoothGATTReadResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTReadResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  dump_bytes_field(out, ESPHOME_PSTR("data"), this->data_ptr_, this->data_len_);
  return out.c_str();
}
const char *BluetoothGATTWriteRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTWriteRequest"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  dump_field(out, ESPHOME_PSTR("response"), this->response);
  dump_bytes_field(out, ESPHOME_PSTR("data"), this->data, this->data_len);
  return out.c_str();
}
const char *BluetoothGATTReadDescriptorRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTReadDescriptorRequest"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  return out.c_str();
}
const char *BluetoothGATTWriteDescriptorRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTWriteDescriptorRequest"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  dump_bytes_field(out, ESPHOME_PSTR("data"), this->data, this->data_len);
  return out.c_str();
}
const char *BluetoothGATTNotifyRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTNotifyRequest"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  dump_field(out, ESPHOME_PSTR("enable"), this->enable);
  return out.c_str();
}
const char *BluetoothGATTNotifyDataResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTNotifyDataResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  dump_bytes_field(out, ESPHOME_PSTR("data"), this->data_ptr_, this->data_len_);
  return out.c_str();
}
const char *BluetoothConnectionsFreeResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothConnectionsFreeResponse"));
  dump_field(out, ESPHOME_PSTR("free"), this->free);
  dump_field(out, ESPHOME_PSTR("limit"), this->limit);
  for (const auto &it : this->allocated) {
    dump_field(out, ESPHOME_PSTR("allocated"), it, 4);
  }
  return out.c_str();
}
const char *BluetoothGATTErrorResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTErrorResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  dump_field(out, ESPHOME_PSTR("error"), this->error);
  return out.c_str();
}
const char *BluetoothGATTWriteResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTWriteResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  return out.c_str();
}
const char *BluetoothGATTNotifyResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothGATTNotifyResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("handle"), this->handle);
  return out.c_str();
}
const char *BluetoothDevicePairingResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothDevicePairingResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("paired"), this->paired);
  dump_field(out, ESPHOME_PSTR("error"), this->error);
  return out.c_str();
}
const char *BluetoothDeviceUnpairingResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothDeviceUnpairingResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("success"), this->success);
  dump_field(out, ESPHOME_PSTR("error"), this->error);
  return out.c_str();
}
const char *BluetoothDeviceClearCacheResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothDeviceClearCacheResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("success"), this->success);
  dump_field(out, ESPHOME_PSTR("error"), this->error);
  return out.c_str();
}
const char *BluetoothScannerStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothScannerStateResponse"));
  dump_field(out, ESPHOME_PSTR("state"), static_cast<enums::BluetoothScannerState>(this->state));
  dump_field(out, ESPHOME_PSTR("mode"), static_cast<enums::BluetoothScannerMode>(this->mode));
  dump_field(out, ESPHOME_PSTR("configured_mode"), static_cast<enums::BluetoothScannerMode>(this->configured_mode));
  return out.c_str();
}
const char *BluetoothScannerSetModeRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothScannerSetModeRequest"));
  dump_field(out, ESPHOME_PSTR("mode"), static_cast<enums::BluetoothScannerMode>(this->mode));
  return out.c_str();
}
#endif
#ifdef USE_VOICE_ASSISTANT
const char *SubscribeVoiceAssistantRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SubscribeVoiceAssistantRequest"));
  dump_field(out, ESPHOME_PSTR("subscribe"), this->subscribe);
  dump_field(out, ESPHOME_PSTR("flags"), this->flags);
  return out.c_str();
}
const char *VoiceAssistantAudioSettings::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantAudioSettings"));
  dump_field(out, ESPHOME_PSTR("noise_suppression_level"), this->noise_suppression_level);
  dump_field(out, ESPHOME_PSTR("auto_gain"), this->auto_gain);
  dump_field(out, ESPHOME_PSTR("volume_multiplier"), this->volume_multiplier);
  return out.c_str();
}
const char *VoiceAssistantRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantRequest"));
  dump_field(out, ESPHOME_PSTR("start"), this->start);
  dump_field(out, ESPHOME_PSTR("conversation_id"), this->conversation_id);
  dump_field(out, ESPHOME_PSTR("flags"), this->flags);
  out.append(2, ' ').append_p(ESPHOME_PSTR("audio_settings")).append(": ");
  this->audio_settings.dump_to(out);
  out.append("\n");
  dump_field(out, ESPHOME_PSTR("wake_word_phrase"), this->wake_word_phrase);
  return out.c_str();
}
const char *VoiceAssistantResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantResponse"));
  dump_field(out, ESPHOME_PSTR("port"), this->port);
  dump_field(out, ESPHOME_PSTR("error"), this->error);
  return out.c_str();
}
const char *VoiceAssistantEventData::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantEventData"));
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("value"), this->value);
  return out.c_str();
}
const char *VoiceAssistantEventResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantEventResponse"));
  dump_field(out, ESPHOME_PSTR("event_type"), static_cast<enums::VoiceAssistantEvent>(this->event_type));
  for (const auto &it : this->data) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("data")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
  return out.c_str();
}
const char *VoiceAssistantAudio::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantAudio"));
  dump_bytes_field(out, ESPHOME_PSTR("data"), this->data, this->data_len);
  dump_field(out, ESPHOME_PSTR("end"), this->end);
  dump_bytes_field(out, ESPHOME_PSTR("data2"), this->data2, this->data2_len);
  return out.c_str();
}
const char *VoiceAssistantTimerEventResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantTimerEventResponse"));
  dump_field(out, ESPHOME_PSTR("event_type"), static_cast<enums::VoiceAssistantTimerEvent>(this->event_type));
  dump_field(out, ESPHOME_PSTR("timer_id"), this->timer_id);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
  dump_field(out, ESPHOME_PSTR("total_seconds"), this->total_seconds);
  dump_field(out, ESPHOME_PSTR("seconds_left"), this->seconds_left);
  dump_field(out, ESPHOME_PSTR("is_active"), this->is_active);
  return out.c_str();
}
const char *VoiceAssistantAnnounceRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantAnnounceRequest"));
  dump_field(out, ESPHOME_PSTR("media_id"), this->media_id);
  dump_field(out, ESPHOME_PSTR("text"), this->text);
  dump_field(out, ESPHOME_PSTR("preannounce_media_id"), this->preannounce_media_id);
  dump_field(out, ESPHOME_PSTR("start_conversation"), this->start_conversation);
  return out.c_str();
}
const char *VoiceAssistantAnnounceFinished::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantAnnounceFinished"));
  dump_field(out, ESPHOME_PSTR("success"), this->success);
  return out.c_str();
}
const char *VoiceAssistantWakeWord::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantWakeWord"));
  dump_field(out, ESPHOME_PSTR("id"), this->id);
  dump_field(out, ESPHOME_PSTR("wake_word"), this->wake_word);
  for (const auto &it : this->trained_languages) {
    dump_field(out, ESPHOME_PSTR("trained_languages"), it, 4);
  }
  return out.c_str();
}
const char *VoiceAssistantExternalWakeWord::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantExternalWakeWord"));
  dump_field(out, ESPHOME_PSTR("id"), this->id);
  dump_field(out, ESPHOME_PSTR("wake_word"), this->wake_word);
  for (const auto &it : this->trained_languages) {
    dump_field(out, ESPHOME_PSTR("trained_languages"), it, 4);
  }
  dump_field(out, ESPHOME_PSTR("model_type"), this->model_type);
  dump_field(out, ESPHOME_PSTR("model_size"), this->model_size);
  dump_field(out, ESPHOME_PSTR("model_hash"), this->model_hash);
  dump_field(out, ESPHOME_PSTR("url"), this->url);
  return out.c_str();
}
const char *VoiceAssistantConfigurationRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantConfigurationRequest"));
  for (const auto &it : this->external_wake_words) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("external_wake_words")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
  return out.c_str();
}
const char *VoiceAssistantConfigurationResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantConfigurationResponse"));
  for (const auto &it : this->available_wake_words) {
    out.append(4, ' ').append_p(ESPHOME_PSTR("available_wake_words")).append(": ");
    it.dump_to(out);
    out.append("\n");
  }
  for (const auto &it : *this->active_wake_words) {
    dump_field(out, ESPHOME_PSTR("active_wake_words"), it, 4);
  }
  dump_field(out, ESPHOME_PSTR("max_active_wake_words"), this->max_active_wake_words);
  return out.c_str();
}
const char *VoiceAssistantSetConfiguration::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("VoiceAssistantSetConfiguration"));
  for (const auto &it : this->active_wake_words) {
    dump_field(out, ESPHOME_PSTR("active_wake_words"), it, 4);
  }
  return out.c_str();
}
#endif
#ifdef USE_ALARM_CONTROL_PANEL
const char *ListEntitiesAlarmControlPanelResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesAlarmControlPanelResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("supported_features"), this->supported_features);
  dump_field(out, ESPHOME_PSTR("requires_code"), this->requires_code);
  dump_field(out, ESPHOME_PSTR("requires_code_to_arm"), this->requires_code_to_arm);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *AlarmControlPanelStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("AlarmControlPanelStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), static_cast<enums::AlarmControlPanelState>(this->state));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *AlarmControlPanelCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("AlarmControlPanelCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("command"), static_cast<enums::AlarmControlPanelStateCommand>(this->command));
  dump_field(out, ESPHOME_PSTR("code"), this->code);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_TEXT
const char *ListEntitiesTextResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesTextResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("min_length"), this->min_length);
  dump_field(out, ESPHOME_PSTR("max_length"), this->max_length);
  dump_field(out, ESPHOME_PSTR("pattern"), this->pattern);
  dump_field(out, ESPHOME_PSTR("mode"), static_cast<enums::TextMode>(this->mode));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *TextStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("TextStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
  dump_field(out, ESPHOME_PSTR("missing_state"), this->missing_state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *TextCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("TextCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("state"), this->state);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_DATETIME_DATE
const char *ListEntitiesDateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesDateResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *DateStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("DateStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("missing_state"), this->missing_state);
  dump_field(out, ESPHOME_PSTR("year"), this->year);
  dump_field(out, ESPHOME_PSTR("month"), this->month);
  dump_field(out, ESPHOME_PSTR("day"), this->day);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *DateCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("DateCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("year"), this->year);
  dump_field(out, ESPHOME_PSTR("month"), this->month);
  dump_field(out, ESPHOME_PSTR("day"), this->day);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_DATETIME_TIME
const char *ListEntitiesTimeResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesTimeResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *TimeStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("TimeStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("missing_state"), this->missing_state);
  dump_field(out, ESPHOME_PSTR("hour"), this->hour);
  dump_field(out, ESPHOME_PSTR("minute"), this->minute);
  dump_field(out, ESPHOME_PSTR("second"), this->second);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *TimeCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("TimeCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("hour"), this->hour);
  dump_field(out, ESPHOME_PSTR("minute"), this->minute);
  dump_field(out, ESPHOME_PSTR("second"), this->second);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_EVENT
const char *ListEntitiesEventResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesEventResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("device_class"), this->device_class);
  for (const auto &it : *this->event_types) {
    dump_field(out, ESPHOME_PSTR("event_types"), it, 4);
  }
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *EventResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("EventResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("event_type"), this->event_type);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_VALVE
const char *ListEntitiesValveResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesValveResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("device_class"), this->device_class);
  dump_field(out, ESPHOME_PSTR("assumed_state"), this->assumed_state);
  dump_field(out, ESPHOME_PSTR("supports_position"), this->supports_position);
  dump_field(out, ESPHOME_PSTR("supports_stop"), this->supports_stop);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *ValveStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ValveStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("position"), this->position);
  dump_field(out, ESPHOME_PSTR("current_operation"), static_cast<enums::ValveOperation>(this->current_operation));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *ValveCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ValveCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("has_position"), this->has_position);
  dump_field(out, ESPHOME_PSTR("position"), this->position);
  dump_field(out, ESPHOME_PSTR("stop"), this->stop);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_DATETIME_DATETIME
const char *ListEntitiesDateTimeResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesDateTimeResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *DateTimeStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("DateTimeStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("missing_state"), this->missing_state);
  dump_field(out, ESPHOME_PSTR("epoch_seconds"), this->epoch_seconds);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *DateTimeCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("DateTimeCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("epoch_seconds"), this->epoch_seconds);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_UPDATE
const char *ListEntitiesUpdateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesUpdateResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
  dump_field(out, ESPHOME_PSTR("device_class"), this->device_class);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *UpdateStateResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("UpdateStateResponse"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("missing_state"), this->missing_state);
  dump_field(out, ESPHOME_PSTR("in_progress"), this->in_progress);
  dump_field(out, ESPHOME_PSTR("has_progress"), this->has_progress);
  dump_field(out, ESPHOME_PSTR("progress"), this->progress);
  dump_field(out, ESPHOME_PSTR("current_version"), this->current_version);
  dump_field(out, ESPHOME_PSTR("latest_version"), this->latest_version);
  dump_field(out, ESPHOME_PSTR("title"), this->title);
  dump_field(out, ESPHOME_PSTR("release_summary"), this->release_summary);
  dump_field(out, ESPHOME_PSTR("release_url"), this->release_url);
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
const char *UpdateCommandRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("UpdateCommandRequest"));
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("command"), static_cast<enums::UpdateCommand>(this->command));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  return out.c_str();
}
#endif
#ifdef USE_ZWAVE_PROXY
const char *ZWaveProxyFrame::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ZWaveProxyFrame"));
  dump_bytes_field(out, ESPHOME_PSTR("data"), this->data, this->data_len);
  return out.c_str();
}
const char *ZWaveProxyRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ZWaveProxyRequest"));
  dump_field(out, ESPHOME_PSTR("type"), static_cast<enums::ZWaveProxyRequestType>(this->type));
  dump_bytes_field(out, ESPHOME_PSTR("data"), this->data, this->data_len);
  return out.c_str();
}
#endif
#ifdef USE_INFRARED
const char *ListEntitiesInfraredResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesInfraredResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  dump_field(out, ESPHOME_PSTR("capabilities"), this->capabilities);
  dump_field(out, ESPHOME_PSTR("receiver_frequency"), this->receiver_frequency);
  return out.c_str();
}
#endif
#if defined(USE_IR_RF) || defined(USE_RADIO_FREQUENCY)
const char *InfraredRFTransmitRawTimingsRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("InfraredRFTransmitRawTimingsRequest"));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("carrier_frequency"), this->carrier_frequency);
  dump_field(out, ESPHOME_PSTR("repeat_count"), this->repeat_count);
  out.append(2, ' ').append_p(ESPHOME_PSTR("timings")).append(": ");
  out.append_p(ESPHOME_PSTR("packed buffer ["));
  append_uint(out, this->timings_count_);
  out.append_p(ESPHOME_PSTR(" values, "));
  append_uint(out, this->timings_length_);
  out.append_p(ESPHOME_PSTR(" bytes]\n"));
  dump_field(out, ESPHOME_PSTR("modulation"), this->modulation);
  return out.c_str();
}
const char *InfraredRFReceiveEvent::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("InfraredRFReceiveEvent"));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  for (const auto &it : *this->timings) {
    dump_field(out, ESPHOME_PSTR("timings"), it, 4);
  }
  return out.c_str();
}
#endif
#ifdef USE_RADIO_FREQUENCY
const char *ListEntitiesRadioFrequencyResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("ListEntitiesRadioFrequencyResponse"));
  dump_field(out, ESPHOME_PSTR("object_id"), this->object_id);
  dump_field(out, ESPHOME_PSTR("key"), this->key);
  dump_field(out, ESPHOME_PSTR("name"), this->name);
#ifdef USE_ENTITY_ICON
  dump_field(out, ESPHOME_PSTR("icon"), this->icon);
#endif
  dump_field(out, ESPHOME_PSTR("disabled_by_default"), this->disabled_by_default);
  dump_field(out, ESPHOME_PSTR("entity_category"), static_cast<enums::EntityCategory>(this->entity_category));
#ifdef USE_DEVICES
  dump_field(out, ESPHOME_PSTR("device_id"), this->device_id);
#endif
  dump_field(out, ESPHOME_PSTR("capabilities"), this->capabilities);
  dump_field(out, ESPHOME_PSTR("frequency_min"), this->frequency_min);
  dump_field(out, ESPHOME_PSTR("frequency_max"), this->frequency_max);
  dump_field(out, ESPHOME_PSTR("supported_modulations"), this->supported_modulations);
  return out.c_str();
}
#endif
#ifdef USE_SERIAL_PROXY
const char *SerialProxyConfigureRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SerialProxyConfigureRequest"));
  dump_field(out, ESPHOME_PSTR("instance"), this->instance);
  dump_field(out, ESPHOME_PSTR("baudrate"), this->baudrate);
  dump_field(out, ESPHOME_PSTR("flow_control"), this->flow_control);
  dump_field(out, ESPHOME_PSTR("parity"), static_cast<enums::SerialProxyParity>(this->parity));
  dump_field(out, ESPHOME_PSTR("stop_bits"), this->stop_bits);
  dump_field(out, ESPHOME_PSTR("data_size"), this->data_size);
  return out.c_str();
}
const char *SerialProxyDataReceived::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SerialProxyDataReceived"));
  dump_field(out, ESPHOME_PSTR("instance"), this->instance);
  dump_bytes_field(out, ESPHOME_PSTR("data"), this->data_ptr_, this->data_len_);
  return out.c_str();
}
const char *SerialProxyWriteRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SerialProxyWriteRequest"));
  dump_field(out, ESPHOME_PSTR("instance"), this->instance);
  dump_bytes_field(out, ESPHOME_PSTR("data"), this->data, this->data_len);
  return out.c_str();
}
const char *SerialProxySetModemPinsRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SerialProxySetModemPinsRequest"));
  dump_field(out, ESPHOME_PSTR("instance"), this->instance);
  dump_field(out, ESPHOME_PSTR("line_states"), this->line_states);
  return out.c_str();
}
const char *SerialProxyGetModemPinsRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SerialProxyGetModemPinsRequest"));
  dump_field(out, ESPHOME_PSTR("instance"), this->instance);
  return out.c_str();
}
const char *SerialProxyGetModemPinsResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SerialProxyGetModemPinsResponse"));
  dump_field(out, ESPHOME_PSTR("instance"), this->instance);
  dump_field(out, ESPHOME_PSTR("line_states"), this->line_states);
  return out.c_str();
}
const char *SerialProxyRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SerialProxyRequest"));
  dump_field(out, ESPHOME_PSTR("instance"), this->instance);
  dump_field(out, ESPHOME_PSTR("type"), static_cast<enums::SerialProxyRequestType>(this->type));
  return out.c_str();
}
const char *SerialProxyRequestResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("SerialProxyRequestResponse"));
  dump_field(out, ESPHOME_PSTR("instance"), this->instance);
  dump_field(out, ESPHOME_PSTR("type"), static_cast<enums::SerialProxyRequestType>(this->type));
  dump_field(out, ESPHOME_PSTR("status"), static_cast<enums::SerialProxyStatus>(this->status));
  dump_field(out, ESPHOME_PSTR("error_message"), this->error_message);
  return out.c_str();
}
#endif
#ifdef USE_BLUETOOTH_PROXY
const char *BluetoothSetConnectionParamsRequest::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothSetConnectionParamsRequest"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("min_interval"), this->min_interval);
  dump_field(out, ESPHOME_PSTR("max_interval"), this->max_interval);
  dump_field(out, ESPHOME_PSTR("latency"), this->latency);
  dump_field(out, ESPHOME_PSTR("timeout"), this->timeout);
  return out.c_str();
}
const char *BluetoothSetConnectionParamsResponse::dump_to(DumpBuffer &out) const {
  MessageDumpHelper helper(out, ESPHOME_PSTR("BluetoothSetConnectionParamsResponse"));
  dump_field(out, ESPHOME_PSTR("address"), this->address);
  dump_field(out, ESPHOME_PSTR("error"), this->error);
  return out.c_str();
}
#endif

}  // namespace esphome::api

#endif  // HAS_PROTO_MESSAGE_DUMP
