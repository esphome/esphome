import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ICON, CONF_ID, CONF_NAME
from esphomeyaml.helpers import App, Pvariable, add, setup_mqtt_component

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

MQTT_SWITCH_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    vol.Optional(CONF_ICON): cv.icon,
})


def setup_mqtt_switch(obj, config):
    if CONF_ICON in config:
        add(obj.set_icon(config[CONF_ICON]))
    setup_mqtt_component(obj, config)


def make_mqtt_switch_for(exp, config):
    rhs = App.make_mqtt_switch_for(exp, config[CONF_NAME])
    mqtt_switch = Pvariable('switch_::MQTTSwitchComponent', config[CONF_ID], rhs)
    setup_mqtt_switch(mqtt_switch, config)
