import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import fan, output
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_OSCILLATION_OUTPUT, CONF_OUTPUT
from esphomeyaml.helpers import App, add, get_variable, variable, setup_component

PLATFORM_SCHEMA = cv.nameable(fan.FAN_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(fan.MakeFan),
    vol.Required(CONF_OUTPUT): cv.use_variable_id(output.BinaryOutput),
    vol.Optional(CONF_OSCILLATION_OUTPUT): cv.use_variable_id(output.BinaryOutput),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for output_ in get_variable(config[CONF_OUTPUT]):
        yield

    rhs = App.make_fan(config[CONF_NAME])
    fan_struct = variable(config[CONF_MAKE_ID], rhs)
    add(fan_struct.Poutput.set_binary(output_))
    if CONF_OSCILLATION_OUTPUT in config:
        for oscillation_output in get_variable(config[CONF_OSCILLATION_OUTPUT]):
            yield
        add(fan_struct.Poutput.set_oscillation(oscillation_output))

    fan.setup_fan(fan_struct.Pstate, fan_struct.Pmqtt, config)
    setup_component(fan_struct.Poutput, config)


def to_hass_config(data, config):
    return fan.core_to_hass_config(data, config)
