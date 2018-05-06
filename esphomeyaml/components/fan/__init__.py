import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_OSCILLATION_COMMAND_TOPIC, CONF_OSCILLATION_STATE_TOPIC, \
    CONF_SPEED_COMMAND_TOPIC, CONF_SPEED_STATE_TOPIC
from esphomeyaml.helpers import add, setup_mqtt_component

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({
    vol.Optional(CONF_OSCILLATION_STATE_TOPIC): cv.publish_topic,
    vol.Optional(CONF_OSCILLATION_COMMAND_TOPIC): cv.subscribe_topic,
}).extend(cv.MQTT_COMMAND_COMPONENT_SCHEMA.schema)


def setup_mqtt_fan(obj, config):
    if CONF_OSCILLATION_STATE_TOPIC in config:
        add(obj.set_custom_oscillation_state_topic(config[CONF_OSCILLATION_STATE_TOPIC]))
    if CONF_OSCILLATION_COMMAND_TOPIC in config:
        add(obj.set_custom_oscillation_command_topic(config[CONF_OSCILLATION_COMMAND_TOPIC]))
    if CONF_SPEED_STATE_TOPIC in config:
        add(obj.set_custom_speed_state_topic(config[CONF_SPEED_STATE_TOPIC]))
    if CONF_SPEED_COMMAND_TOPIC in config:
        add(obj.set_custom_speed_command_topic(config[CONF_SPEED_COMMAND_TOPIC]))
    setup_mqtt_component(obj, config)


BUILD_FLAGS = '-DUSE_FAN'
