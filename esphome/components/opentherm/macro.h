#pragma once

// Ensure that all component macros are defined, even if the component is not used
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

// Request builders
#define OPENTHERM_MESSAGE_WRITE_MESSAGE(msg) \
  case MessageId::msg: { \
    data.type = MessageType::WRITE_DATA; \
    data.id = request_id;
#define OPENTHERM_MESSAGE_WRITE_ENTITY(key, msg_data) message_data::write_##msg_data(this->key->state, data);
#define OPENTHERM_MESSAGE_WRITE_POSTSCRIPT \
  return data; \
  }

#define OPENTHERM_MESSAGE_READ_MESSAGE(msg) \
  case MessageId::msg: \
    data.type = MessageType::READ_DATA; \
    data.id = request_id; \
    return data;

// Define the handler helpers to publish the results to all sensors
#define OPENTHERM_MESSAGE_RESPONSE_MESSAGE(msg) case MessageId::msg:
#define OPENTHERM_MESSAGE_RESPONSE_ENTITY(key, msg_data) this->key->publish_state(message_data::parse_##msg_data(data));
#define OPENTHERM_MESSAGE_RESPONSE_POSTSCRIPT break;

// Config dump
#define ID(x) x
#define SHOW2(x) #x
#define SHOW(x) SHOW2(x)

// Misc
#define OPENTHERM_IGNORE_1(x)
#define OPENTHERM_IGNORE_2(x, y)