import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_MQTT_ID, CONF_OSCILLATION_COMMAND_TOPIC, \
    CONF_OSCILLATION_STATE_TOPIC, CONF_SPEED_COMMAND_TOPIC, CONF_SPEED_STATE_TOPIC
from esphomeyaml.helpers import Application, Pvariable, add, esphomelib_ns, setup_mqtt_component

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

FAN_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID('fan'): cv.register_variable_id,
    cv.GenerateID('mqtt_fan', CONF_MQTT_ID): cv.register_variable_id,
    vol.Optional(CONF_OSCILLATION_STATE_TOPIC): cv.publish_topic,
    vol.Optional(CONF_OSCILLATION_COMMAND_TOPIC): cv.subscribe_topic,
})

fan_ns = esphomelib_ns.namespace('fan')
FanState = fan_ns.FanState
MQTTFanComponent = fan_ns.MQTTFanComponent
MakeFan = Application.MakeFan


def setup_fan_core_(fan_var, mqtt_var, config):
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
    fan_var = Pvariable(FanState, config[CONF_ID], fan_obj, has_side_effects=False)
    mqtt_var = Pvariable(MQTTFanComponent, config[CONF_MQTT_ID], mqtt_obj, has_side_effects=False)
    setup_fan_core_(fan_var, mqtt_var, config)


BUILD_FLAGS = '-DUSE_FAN'
