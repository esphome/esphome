#pragma once

#include "esphome/core/component.h"
#include "esphome/core/log.h"

#include "OpenTherm.h"

#include "esphome/components/sensor/sensor.h"
#include "esphome/components/binary_sensor/binary_sensor.h"

#include "switch.h"
#include "number.h"
#include "output.h"

#include <unordered_map>
#include <unordered_set>

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
    int in_pin, out_pin;
    // The OpenTherm interface from @ihormelnyk's library
    OpenTherm* ot;

    // Use macros to create fields for every entity specified in the ESPHome configuration
    #define OPENTHERM_DECLARE_SENSOR(entity) sensor::Sensor* entity;
    OPENTHERM_SENSOR_LIST(OPENTHERM_DECLARE_SENSOR, )

    #define OPENTHERM_DECLARE_BINARY_SENSOR(entity) binary_sensor::BinarySensor* entity;
    OPENTHERM_BINARY_SENSOR_LIST(OPENTHERM_DECLARE_BINARY_SENSOR, )

    #define OPENTHERM_DECLARE_SWITCH(entity) OpenthermSwitch* entity;
    OPENTHERM_SWITCH_LIST(OPENTHERM_DECLARE_SWITCH, )

    #define OPENTHERM_DECLARE_NUMBER(entity) OpenthermNumber* entity;
    OPENTHERM_NUMBER_LIST(OPENTHERM_DECLARE_NUMBER, )

    #define OPENTHERM_DECLARE_OUTPUT(entity) OpenthermOutput* entity;
    OPENTHERM_OUTPUT_LIST(OPENTHERM_DECLARE_OUTPUT, )

    #define OPENTHERM_DECLARE_INPUT_SENSOR(entity) sensor::Sensor* entity;
    OPENTHERM_INPUT_SENSOR_LIST(OPENTHERM_DECLARE_INPUT_SENSOR, )

    // The set of initial messages to send on starting communication with the boiler
    std::unordered_set<OpenThermMessageID> initial_messages;
    // and the repeating messages which are sent repeatedly to update various sensors
    // and boiler parameters (like the setpoint).
    std::unordered_set<OpenThermMessageID> repeating_messages;
    // Indicates if we are still working on the initial requests or not
    bool initializing = true;
    // Index for the current request in one of the _requests sets.
    std::unordered_set<OpenThermMessageID>::const_iterator current_message_iterator;

    // Create OpenTherm messages based on the message id
    unsigned int build_request(OpenThermMessageID request_id);

    // Callbacks to pass to OpenTherm interface for globally defined interrupts
    void(*handle_interrupt_callback)();
	void(*process_response_callback)(unsigned long, OpenThermResponseStatus);

public:
    // Constructor with references to the global interrupt handlers
    OpenthermHub(void(*handle_interrupt_callback)(void), void(*process_response_callback)(unsigned long, OpenThermResponseStatus));

    // Interrupt handler, which notifies the OpenTherm interface of an interrupt
    void IRAM_ATTR handle_interrupt();

    // Handle responses from the OpenTherm interface
    void process_response(unsigned long response, OpenThermResponseStatus status);

    // Setters for the input and output OpenTherm interface pins
    void set_in_pin(int in_pin) { this->in_pin = in_pin; }
    void set_out_pin(int out_pin) { this->out_pin = out_pin; }

    #define OPENTHERM_SET_SENSOR(entity) void set_ ## entity(sensor::Sensor* sensor) { this->entity = sensor; }
    OPENTHERM_SENSOR_LIST(OPENTHERM_SET_SENSOR, )

    #define OPENTHERM_SET_BINARY_SENSOR(entity) void set_ ## entity(binary_sensor::BinarySensor* binary_sensor) { this->entity = binary_sensor; }
    OPENTHERM_BINARY_SENSOR_LIST(OPENTHERM_SET_BINARY_SENSOR, )

    #define OPENTHERM_SET_SWITCH(entity) void set_ ## entity(OpenthermSwitch* sw) { this->entity = sw; }
    OPENTHERM_SWITCH_LIST(OPENTHERM_SET_SWITCH, )

    #define OPENTHERM_SET_NUMBER(entity) void set_ ## entity(OpenthermNumber* number) { this->entity = number; }
    OPENTHERM_NUMBER_LIST(OPENTHERM_SET_NUMBER, )

    #define OPENTHERM_SET_OUTPUT(entity) void set_ ## entity(OpenthermOutput* output) { this->entity = output; }
    OPENTHERM_OUTPUT_LIST(OPENTHERM_SET_OUTPUT, )

    #define OPENTHERM_SET_INPUT_SENSOR(entity) void set_ ## entity(sensor::Sensor* sensor) { this->entity = sensor; }
    OPENTHERM_INPUT_SENSOR_LIST(OPENTHERM_SET_INPUT_SENSOR, )

    // Add a request to the set of initial requests
    void add_initial_message(OpenThermMessageID message_id) { this->initial_messages.insert(message_id); }
    // Add a request to the set of repeating requests. Note that a large number of repeating
    // requests will slow down communication with the boiler. Each request may take up to 1 second,
    // so with all sensors enabled, it may take about half a minute before a change in setpoint
    // will be processed.
    void add_repeating_message(OpenThermMessageID message_id) { this->repeating_messages.insert(message_id); }

    // There are five status variables, which can either be set as a simple variable,
    // or using a switch. ch_enable and dhw_enable default to true, the others to false.
    bool ch_enable = true, dhw_enable = true, cooling_enable, otc_active, ch2_active;

    // Synchronous communication mode prevents other components from disabling interrupts while
    // we are talking to the boiler. Enable if you experience random intermittent invalid response errors.
    // Very likely to happen while using Dallas temperature sensors.
    bool sync_mode = false;

    // Setters for the status variables
    void set_ch_enable(bool ch_enable) { this->ch_enable = ch_enable; }
    void set_dhw_enable(bool dhw_enable) { this->dhw_enable = dhw_enable; }
    void set_cooling_enable(bool cooling_enable) { this->cooling_enable = cooling_enable; }
    void set_otc_active(bool otc_active) { this->otc_active = otc_active; }
    void set_ch2_active(bool ch2_active) { this->ch2_active = ch2_active; }
    void set_sync_mode(bool sync_mode) { this->sync_mode = sync_mode; }
    
    float get_setup_priority() const override{
        return setup_priority::HARDWARE;
    }

    void setup() override;
    void on_shutdown() override;
    void loop() override;
    void dump_config() override;
};

} // namespace opentherm
} // namespace esphome
