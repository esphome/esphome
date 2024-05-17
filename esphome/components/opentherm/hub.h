#pragma once

#include "esphome/core/defines.h"
#include "esphome/core/hal.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include "opentherm.h"

#ifdef OPENTHERM_USE_SENSOR
#include "esphome/components/sensor/sensor.h"
#endif

#ifdef OPENTHERM_USE_BINARY_SENSOR
#include "esphome/components/binary_sensor/binary_sensor.h"
#endif

#ifdef OPENTHERM_USE_SWITCH
#include "esphome/components/opentherm/switch/switch.h"
#endif

#ifdef OPENTHERM_USE_OUTPUT
#include "esphome/components/opentherm/output/output.h"
#endif

#ifdef OPENTHERM_USE_NUMBER
#include "esphome/components/opentherm/number/number.h"
#endif

#include <unordered_map>
#include <unordered_set>
#include <functional>

#define OT_TAG "opentherm"

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

namespace esphome {
namespace opentherm {

// OpenTherm component for ESPHome
class OpenthermHub : public Component {
 protected:
  // Communication pins for the OpenTherm interface
  InternalGPIOPin *in_pin_, *out_pin_;
  // The OpenTherm interface
  OpenTherm *opentherm_;

// Use macros to create fields for every entity specified in the ESPHome configuration
#define OPENTHERM_DECLARE_SENSOR(entity) sensor::Sensor *entity;
  OPENTHERM_SENSOR_LIST(OPENTHERM_DECLARE_SENSOR, )

#define OPENTHERM_DECLARE_BINARY_SENSOR(entity) binary_sensor::BinarySensor *entity;
  OPENTHERM_BINARY_SENSOR_LIST(OPENTHERM_DECLARE_BINARY_SENSOR, )

#define OPENTHERM_DECLARE_SWITCH(entity) OpenthermSwitch *entity;
  OPENTHERM_SWITCH_LIST(OPENTHERM_DECLARE_SWITCH, )

#define OPENTHERM_DECLARE_NUMBER(entity) OpenthermNumber *entity;
  OPENTHERM_NUMBER_LIST(OPENTHERM_DECLARE_NUMBER, )

#define OPENTHERM_DECLARE_OUTPUT(entity) OpenthermOutput *entity;
  OPENTHERM_OUTPUT_LIST(OPENTHERM_DECLARE_OUTPUT, )

#define OPENTHERM_DECLARE_INPUT_SENSOR(entity) sensor::Sensor *entity;
  OPENTHERM_INPUT_SENSOR_LIST(OPENTHERM_DECLARE_INPUT_SENSOR, )

  // The set of initial messages to send on starting communication with the boiler
  std::unordered_set<MessageId> initial_messages_;
  // and the repeating messages which are sent repeatedly to update various sensors
  // and boiler parameters (like the setpoint).
  std::unordered_set<MessageId> repeating_messages_;
  // Indicates if we are still working on the initial requests or not
  bool initializing_ = true;
  // Index for the current request in one of the _requests sets.
  std::unordered_set<MessageId>::const_iterator current_message_iterator_;

  uint32_t last_conversation_start_ = 0;
  uint32_t last_conversation_end_ = 0;

  // Create OpenTherm messages based on the message id
  OpenthermData build_request_(MessageId request_id);

  template<typename F> bool spin_wait_(uint32_t timeout, F func) {
    auto start_time = millis();
    while (func()) {
      yield();
      auto cur_time = millis();
      if (cur_time - start_time >= timeout) {
        return false;
      }
    }
    return true;
  }

 public:
  // Constructor with references to the global interrupt handlers
  OpenthermHub();

  // Handle responses from the OpenTherm interface
  void process_response(OpenthermData &data);

  // Setters for the input and output OpenTherm interface pins
  void set_in_pin(InternalGPIOPin *in_pin) { this->in_pin_ = in_pin; }
  void set_out_pin(InternalGPIOPin *out_pin) { this->out_pin_ = out_pin; }

#define OPENTHERM_SET_SENSOR(entity) \
  void set_##entity(sensor::Sensor *sensor) { this->entity = sensor; }
  OPENTHERM_SENSOR_LIST(OPENTHERM_SET_SENSOR, )

#define OPENTHERM_SET_BINARY_SENSOR(entity) \
  void set_##entity(binary_sensor::BinarySensor *binary_sensor) { this->entity = binary_sensor; }
  OPENTHERM_BINARY_SENSOR_LIST(OPENTHERM_SET_BINARY_SENSOR, )

#define OPENTHERM_SET_SWITCH(entity) \
  void set_##entity(OpenthermSwitch *sw) { this->entity = sw; }
  OPENTHERM_SWITCH_LIST(OPENTHERM_SET_SWITCH, )

#define OPENTHERM_SET_NUMBER(entity) \
  void set_##entity(OpenthermNumber *number) { this->entity = number; }
  OPENTHERM_NUMBER_LIST(OPENTHERM_SET_NUMBER, )

#define OPENTHERM_SET_OUTPUT(entity) \
  void set_##entity(OpenthermOutput *output) { this->entity = output; }
  OPENTHERM_OUTPUT_LIST(OPENTHERM_SET_OUTPUT, )

#define OPENTHERM_SET_INPUT_SENSOR(entity) \
  void set_##entity(sensor::Sensor *sensor) { this->entity = sensor; }
  OPENTHERM_INPUT_SENSOR_LIST(OPENTHERM_SET_INPUT_SENSOR, )

  // Add a request to the set of initial requests
  void add_initial_message(MessageId message_id) { this->initial_messages_.insert(message_id); }
  // Add a request to the set of repeating requests. Note that a large number of repeating
  // requests will slow down communication with the boiler. Each request may take up to 1 second,
  // so with all sensors enabled, it may take about half a minute before a change in setpoint
  // will be processed.
  void add_repeating_message(MessageId message_id) { this->repeating_messages_.insert(message_id); }

  // There are five status variables, which can either be set as a simple variable,
  // or using a switch. ch_enable and dhw_enable default to true, the others to false.
  bool ch_enable = true, dhw_enable = true, cooling_enable = false, otc_active = false, ch2_active = false;

  // Setters for the status variables
  void set_ch_enable(bool value) { this->ch_enable = value; }
  void set_dhw_enable(bool value) { this->dhw_enable = value; }
  void set_cooling_enable(bool value) { this->cooling_enable = value; }
  void set_otc_active(bool value) { this->otc_active = value; }
  void set_ch2_active(bool value) { this->ch2_active = value; }

  float get_setup_priority() const override { return setup_priority::HARDWARE; }

  void setup() override;
  void on_shutdown() override;
  void loop() override;
  void dump_config() override;
};

}  // namespace opentherm
}  // namespace esphome
