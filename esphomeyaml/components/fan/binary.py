import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import fan
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_OSCILLATION_OUTPUT, CONF_OUTPUT
from esphomeyaml.helpers import App, add, get_variable, variable

PLATFORM_SCHEMA = fan.PLATFORM_SCHEMA.extend({
    cv.GenerateID('binary_fan', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_OUTPUT): cv.variable_id,
    vol.Optional(CONF_OSCILLATION_OUTPUT): cv.variable_id,
}).extend(fan.FAN_SCHEMA.schema)


def to_code(config):
    output = get_variable(config[CONF_OUTPUT])
    rhs = App.make_fan(config[CONF_NAME])
    fan_struct = variable(fan.MakeFan, config[CONF_MAKE_ID], rhs)
    add(fan_struct.Poutput.set_binary(output))
    if CONF_OSCILLATION_OUTPUT in config:
        oscillation_output = get_variable(config[CONF_OSCILLATION_OUTPUT])
        add(fan_struct.Poutput.set_oscillation(oscillation_output))

    fan.setup_fan(fan_struct.Pstate, fan_struct.Pmqtt, config)
