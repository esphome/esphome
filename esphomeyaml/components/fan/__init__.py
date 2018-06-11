import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_MQTT_ID, CONF_OSCILLATION_COMMAND_TOPIC, \
    CONF_OSCILLATION_STATE_TOPIC, CONF_SPEED_COMMAND_TOPIC, CONF_SPEED_STATE_TOPIC, CONF_INTERNAL
from esphomeyaml.helpers import Application, Pvariable, add, esphomelib_ns, setup_mqtt_component

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

fan_ns = esphomelib_ns.namespace('fan')
FanState = fan_ns.FanState
MQTTFanComponent = fan_ns.MQTTFanComponent
MakeFan = Application.MakeFan
TurnOnAction = fan_ns.TurnOnAction
TurnOffAction = fan_ns.TurnOffAction
ToggleAction = fan_ns.ToggleAction
FanSpeed = fan_ns.FanSpeed
FAN_SPEED_OFF = fan_ns.FAN_SPEED_OFF
FAN_SPEED_LOW = fan_ns.FAN_SPEED_LOW
FAN_SPEED_MEDIUM = fan_ns.FAN_SPEED_MEDIUM
FAN_SPEED_HIGH = fan_ns.FAN_SPEED_HIGH

FAN_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(FanState),
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTFanComponent),
    vol.Optional(CONF_OSCILLATION_STATE_TOPIC): cv.publish_topic,
    vol.Optional(CONF_OSCILLATION_COMMAND_TOPIC): cv.subscribe_topic,
})

FAN_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(FAN_SCHEMA.schema)


FAN_SPEEDS = {
    'OFF': FAN_SPEED_OFF,
    'LOW': FAN_SPEED_LOW,
    'MEDIUM': FAN_SPEED_MEDIUM,
    'HIGH': FAN_SPEED_HIGH,
}


def validate_fan_speed(value):
    return vol.All(vol.Upper, cv.one_of(*FAN_SPEEDS))(value)


def setup_fan_core_(fan_var, mqtt_var, config):
    if CONF_INTERNAL in config:
        add(fan_var.set_internal(config[CONF_INTERNAL]))

    if CONF_OSCILLATION_STATE_TOPIC in config:
        add(mqtt_var.set_custom_oscillation_state_topic(config[CONF_OSCILLATION_STATE_TOPIC]))
    if CONF_OSCILLATION_COMMAND_TOPIC in config:
        add(mqtt_var.set_custom_oscillation_command_topic(config[CONF_OSCILLATION_COMMAND_TOPIC]))
    if CONF_SPEED_STATE_TOPIC in config:
        add(mqtt_var.set_custom_speed_state_topic(config[CONF_SPEED_STATE_TOPIC]))
    if CONF_SPEED_COMMAND_TOPIC in config:
        add(mqtt_var.set_custom_speed_command_topic(config[CONF_SPEED_COMMAND_TOPIC]))
    setup_mqtt_component(mqtt_var, config)


def setup_fan(fan_obj, mqtt_obj, config):
    fan_var = Pvariable(config[CONF_ID], fan_obj, has_side_effects=False)
    mqtt_var = Pvariable(config[CONF_MQTT_ID], mqtt_obj, has_side_effects=False)
    setup_fan_core_(fan_var, mqtt_var, config)


BUILD_FLAGS = '-DUSE_FAN'
