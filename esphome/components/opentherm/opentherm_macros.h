#pragma once
namespace esphome {
namespace opentherm {

// ===== hub.h macros =====

// *_LIST macros will be generated in defines.h if at least one sensor from each platform is used.
// These lists will look like this:
// #define OPENTHERM_BINARY_SENSOR_LIST(F, sep) F(sensor_1) sep F(sensor_2)
// These lists will be used in hub.h to define sensor fields (passing macros like OPENTHERM_DECLARE_SENSOR as F)
// and setters (passing macros like OPENTHERM_SET_SENSOR as F) (see below)
// In order for things not to break, we define empty lists here in case some platforms are not used in config.
#ifndef OPENTHERM_SENSOR_LIST
#define OPENTHERM_SENSOR_LIST(F, sep)
#endif
#ifndef OPENTHERM_BINARY_SENSOR_LIST
#define OPENTHERM_BINARY_SENSOR_LIST(F, sep)
#endif
#ifndef OPENTHERM_SWITCH_LIST
#define OPENTHERM_SWITCH_LIST(F, sep)
#endif
#ifndef OPENTHERM_NUMBER_LIST
#define OPENTHERM_NUMBER_LIST(F, sep)
#endif
#ifndef OPENTHERM_OUTPUT_LIST
#define OPENTHERM_OUTPUT_LIST(F, sep)
#endif
#ifndef OPENTHERM_INPUT_SENSOR_LIST
#define OPENTHERM_INPUT_SENSOR_LIST(F, sep)
#endif

// Use macros to create fields for every entity specified in the ESPHome configuration
#define OPENTHERM_DECLARE_SENSOR(entity) sensor::Sensor *entity;
#define OPENTHERM_DECLARE_BINARY_SENSOR(entity) binary_sensor::BinarySensor *entity;
#define OPENTHERM_DECLARE_SWITCH(entity) OpenthermSwitch *entity;
#define OPENTHERM_DECLARE_NUMBER(entity) OpenthermNumber *entity;
#define OPENTHERM_DECLARE_OUTPUT(entity) OpenthermOutput *entity;
#define OPENTHERM_DECLARE_INPUT_SENSOR(entity) sensor::Sensor *entity;

// Setter macros
#define OPENTHERM_SET_SENSOR(entity) \
  void set_##entity(sensor::Sensor *sensor) { this->entity = sensor; }

#define OPENTHERM_SET_BINARY_SENSOR(entity) \
  void set_##entity(binary_sensor::BinarySensor *binary_sensor) { this->entity = binary_sensor; }

#define OPENTHERM_SET_SWITCH(entity) \
  void set_##entity(OpenthermSwitch *sw) { this->entity = sw; }

#define OPENTHERM_SET_NUMBER(entity) \
  void set_##entity(OpenthermNumber *number) { this->entity = number; }

#define OPENTHERM_SET_OUTPUT(entity) \
  void set_##entity(OpenthermOutput *output) { this->entity = output; }

#define OPENTHERM_SET_INPUT_SENSOR(entity) \
  void set_##entity(sensor::Sensor *sensor) { this->entity = sensor; }

// ===== hub.cpp macros =====

// *_MESSAGE_HANDLERS are generated in defines.h and look like this:
// OPENTHERM_NUMBER_MESSAGE_HANDLERS(MESSAGE, ENTITY, entity_sep, postscript, msg_sep) MESSAGE(COOLING_CONTROL)
// ENTITY(cooling_control_number, f88) postscript msg_sep They contain placeholders for message part and entities parts,
// since one message can contain multiple entities. MESSAGE part is substituted with OPENTHERM_MESSAGE_WRITE_MESSAGE,
// OPENTHERM_MESSAGE_READ_MESSAGE or OPENTHERM_MESSAGE_RESPONSE_MESSAGE. ENTITY part is substituted with
// OPENTHERM_MESSAGE_WRITE_ENTITY or OPENTHERM_MESSAGE_RESPONSE_ENTITY. OPENTHERM_IGNORE is used for sensor read
// requests since no data needs to be sent or processed, just the data id.

// In order for things not to break, we define empty lists here in case some platforms are not used in config.
#ifndef OPENTHERM_SENSOR_MESSAGE_HANDLERS
#define OPENTHERM_SENSOR_MESSAGE_HANDLERS(MESSAGE, ENTITY, entity_sep, postscript, msg_sep)
#endif
#ifndef OPENTHERM_BINARY_SENSOR_MESSAGE_HANDLERS
#define OPENTHERM_BINARY_SENSOR_MESSAGE_HANDLERS(MESSAGE, ENTITY, entity_sep, postscript, msg_sep)
#endif
#ifndef OPENTHERM_SWITCH_MESSAGE_HANDLERS
#define OPENTHERM_SWITCH_MESSAGE_HANDLERS(MESSAGE, ENTITY, entity_sep, postscript, msg_sep)
#endif
#ifndef OPENTHERM_NUMBER_MESSAGE_HANDLERS
#define OPENTHERM_NUMBER_MESSAGE_HANDLERS(MESSAGE, ENTITY, entity_sep, postscript, msg_sep)
#endif
#ifndef OPENTHERM_OUTPUT_MESSAGE_HANDLERS
#define OPENTHERM_OUTPUT_MESSAGE_HANDLERS(MESSAGE, ENTITY, entity_sep, postscript, msg_sep)
#endif
#ifndef OPENTHERM_INPUT_SENSOR_MESSAGE_HANDLERS
#define OPENTHERM_INPUT_SENSOR_MESSAGE_HANDLERS(MESSAGE, ENTITY, entity_sep, postscript, msg_sep)
#endif

// Write data request builders
#define OPENTHERM_MESSAGE_WRITE_MESSAGE(msg) \
  case MessageId::msg: { \
    data.type = MessageType::WRITE_DATA; \
    data.id = request_id;
#define OPENTHERM_MESSAGE_WRITE_ENTITY(key, msg_data) message_data::write_##msg_data(this->key->state, data);
#define OPENTHERM_MESSAGE_WRITE_POSTSCRIPT \
  return data; \
  }

// Read data request builder
#define OPENTHERM_MESSAGE_READ_MESSAGE(msg) \
  case MessageId::msg: \
    data.type = MessageType::READ_DATA; \
    data.id = request_id; \
    return data;

// Data processing builders
#define OPENTHERM_MESSAGE_RESPONSE_MESSAGE(msg) case MessageId::msg:
#define OPENTHERM_MESSAGE_RESPONSE_ENTITY(key, msg_data) this->key->publish_state(message_data::parse_##msg_data(data));
#define OPENTHERM_MESSAGE_RESPONSE_POSTSCRIPT break;

#define OPENTHERM_IGNORE(x, y)

// Default macros for STATUS entities
#ifndef OPENTHERM_READ_ch_enable
#define OPENTHERM_READ_ch_enable true
#endif
#ifndef OPENTHERM_READ_dhw_enable
#define OPENTHERM_READ_dhw_enable true
#endif
#ifndef OPENTHERM_READ_t_set
#define OPENTHERM_READ_t_set 0.0
#endif
#ifndef OPENTHERM_READ_cooling_enable
#define OPENTHERM_READ_cooling_enable false
#endif
#ifndef OPENTHERM_READ_cooling_control
#define OPENTHERM_READ_cooling_control 0.0
#endif
#ifndef OPENTHERM_READ_otc_active
#define OPENTHERM_READ_otc_active false
#endif
#ifndef OPENTHERM_READ_ch2_active
#define OPENTHERM_READ_ch2_active false
#endif
#ifndef OPENTHERM_READ_t_set_ch2
#define OPENTHERM_READ_t_set_ch2 0.0
#endif
#ifndef OPENTHERM_READ_summer_mode_active
#define OPENTHERM_READ_summer_mode_active false
#endif
#ifndef OPENTHERM_READ_dhw_block
#define OPENTHERM_READ_dhw_block false
#endif

// These macros utilize the structure of *_LIST macros in order
#define ID(x) x
#define SHOW_INNER(x) #x
#define SHOW(x) SHOW_INNER(x)

}  // namespace opentherm
}  // namespace esphome
