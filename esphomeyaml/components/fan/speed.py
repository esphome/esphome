import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import fan
from esphomeyaml.const import CONF_HIGH, CONF_LOW, CONF_MAKE_ID, CONF_MEDIUM, CONF_NAME, \
    CONF_OSCILLATION_OUTPUT, CONF_OUTPUT, CONF_SPEED, CONF_SPEED_COMMAND_TOPIC, \
    CONF_SPEED_STATE_TOPIC
from esphomeyaml.helpers import App, add, get_variable, variable

PLATFORM_SCHEMA = cv.nameable(fan.FAN_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(fan.MakeFan),
    vol.Required(CONF_OUTPUT): cv.use_variable_id(None),
    vol.Optional(CONF_SPEED_STATE_TOPIC): cv.publish_topic,
    vol.Optional(CONF_SPEED_COMMAND_TOPIC): cv.subscribe_topic,
    vol.Optional(CONF_OSCILLATION_OUTPUT): cv.use_variable_id(None),
    vol.Optional(CONF_SPEED): vol.Schema({
        vol.Required(CONF_LOW): cv.percentage,
        vol.Required(CONF_MEDIUM): cv.percentage,
        vol.Required(CONF_HIGH): cv.percentage,
    }),
}))


def to_code(config):
    output = None
    for output in get_variable(config[CONF_OUTPUT]):
        yield
    rhs = App.make_fan(config[CONF_NAME])
    fan_struct = variable(config[CONF_MAKE_ID], rhs)
    if CONF_SPEED in config:
        speeds = config[CONF_SPEED]
        add(fan_struct.Poutput.set_speed(output, 0.0,
                                         speeds[CONF_LOW],
                                         speeds[CONF_MEDIUM],
                                         speeds[CONF_HIGH]))
    else:
        add(fan_struct.Poutput.set_speed(output))

    if CONF_OSCILLATION_OUTPUT in config:
        oscillation_output = None
        for oscillation_output in get_variable(config[CONF_OSCILLATION_OUTPUT]):
            yield
        add(fan_struct.Poutput.set_oscillation(oscillation_output))

    fan.setup_fan(fan_struct.Pstate, fan_struct.Pmqtt, config)
