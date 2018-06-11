import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ICON, CONF_ID, CONF_INVERTED, CONF_MQTT_ID, CONF_INTERNAL
from esphomeyaml.helpers import App, Pvariable, add, esphomelib_ns, setup_mqtt_component

PLATFORM_SCHEMA = cv.PLATFORM_SCHEMA.extend({

})

switch_ns = esphomelib_ns.namespace('switch_')
Switch = switch_ns.Switch
MQTTSwitchComponent = switch_ns.MQTTSwitchComponent
ToggleAction = switch_ns.ToggleAction
TurnOffAction = switch_ns.TurnOffAction
TurnOnAction = switch_ns.TurnOnAction

SWITCH_SCHEMA = cv.MQTT_COMMAND_COMPONENT_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(Switch),
    cv.GenerateID(CONF_MQTT_ID): cv.declare_variable_id(MQTTSwitchComponent),
    vol.Optional(CONF_ICON): cv.icon,
    vol.Optional(CONF_INVERTED): cv.boolean,
})

SWITCH_PLATFORM_SCHEMA = PLATFORM_SCHEMA.extend(SWITCH_SCHEMA.schema)


def setup_switch_core_(switch_var, mqtt_var, config):
    if CONF_INTERNAL in config:
        add(switch_var.set_internal(config[CONF_INTERNAL]))
    if CONF_ICON in config:
        add(switch_var.set_icon(config[CONF_ICON]))
    if CONF_INVERTED in config:
        add(switch_var.set_inverted(config[CONF_INVERTED]))

    setup_mqtt_component(mqtt_var, config)


def setup_switch(switch_obj, mqtt_obj, config):
    switch_var = Pvariable(config[CONF_ID], switch_obj, has_side_effects=False)
    mqtt_var = Pvariable(config[CONF_MQTT_ID], mqtt_obj, has_side_effects=False)
    setup_switch_core_(switch_var, mqtt_var, config)


def register_switch(var, config):
    switch_var = Pvariable(config[CONF_ID], var, has_side_effects=True)
    rhs = App.register_switch(switch_var)
    mqtt_var = Pvariable(config[CONF_MQTT_ID], rhs, has_side_effects=True)
    setup_switch_core_(switch_var, mqtt_var, config)


BUILD_FLAGS = '-DUSE_SWITCH'
