import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ICON, CONF_INVERTED, CONF_MQTT_ID
from esphomeyaml.helpers import App, Pvariable, add, setup_mqtt_component

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

MQTT_SWITCH_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    vol.Optional(CONF_ICON): cv.icon,
    vol.Optional(CONF_INVERTED): cv.boolean,
})

MQTT_SWITCH_ID_SCHEMA = MQTT_SWITCH_SCHEMA.extend({
    cv.GenerateID('mqtt_switch', CONF_MQTT_ID): cv.register_variable_id,
})


def setup_mqtt_switch(obj, config):
    setup_mqtt_component(obj, config)


def setup_switch(obj, config):
    if CONF_ICON in config:
        add(obj.set_icon(config[CONF_ICON]))
    if CONF_INVERTED in config:
        add(obj.set_inverted(config[CONF_INVERTED]))


def register_switch(var, config):
    setup_switch(var, config)
    rhs = App.register_switch(var)
    mqtt_switch = Pvariable('switch_::MQTTSwitchComponent', config[CONF_MQTT_ID], rhs)
    setup_mqtt_switch(mqtt_switch, config)


BUILD_FLAGS = '-DUSE_SWITCH'
