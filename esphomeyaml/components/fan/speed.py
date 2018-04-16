import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import fan
from esphomeyaml.const import CONF_HIGH, CONF_ID, CONF_LOW, \
    CONF_MEDIUM, CONF_NAME, CONF_OSCILLATION_OUTPUT, CONF_OUTPUT, CONF_SPEED, \
    CONF_SPEED_COMMAND_TOPIC, CONF_SPEED_STATE_TOPIC
from esphomeyaml.helpers import App, add, get_variable, variable

PLATFORM_SCHEMA = fan.PLATFORM_SCHEMA.extend({
    cv.GenerateID('speed_fan'): cv.register_variable_id,
    vol.Required(CONF_OUTPUT): cv.variable_id,
    vol.Optional(CONF_SPEED_STATE_TOPIC): cv.publish_topic,
    vol.Optional(CONF_SPEED_COMMAND_TOPIC): cv.subscribe_topic,
    vol.Optional(CONF_OSCILLATION_OUTPUT): cv.variable_id,
    vol.Optional(CONF_SPEED): vol.Schema({
        vol.Required(CONF_LOW): cv.zero_to_one_float,
        vol.Required(CONF_MEDIUM): cv.zero_to_one_float,
        vol.Required(CONF_HIGH): cv.zero_to_one_float,
    }),
})


def to_code(config):
    output = get_variable(config[CONF_OUTPUT])
    rhs = App.make_fan(config[CONF_NAME])
    fan_struct = variable('Application::MakeFan', config[CONF_ID], rhs)
    if CONF_SPEED in config:
        speeds = config[CONF_SPEED]
        add(fan_struct.Poutput.set_speed(output, 0.0,
                                         speeds[CONF_LOW],
                                         speeds[CONF_MEDIUM],
                                         speeds[CONF_HIGH]))
    else:
        add(fan_struct.Poutput.set_speed(output))

    if CONF_OSCILLATION_OUTPUT in config:
        oscillation_output = get_variable(config[CONF_OSCILLATION_OUTPUT])
        add(fan_struct.Poutput.set_oscillation(oscillation_output))
    fan.setup_mqtt_fan(fan_struct.Pmqtt, config)
