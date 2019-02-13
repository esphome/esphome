import voluptuous as vol

from esphome.components import light, output
import esphome.config_validation as cv
from esphome.const import CONF_DEFAULT_TRANSITION_LENGTH, CONF_EFFECTS, CONF_GAMMA_CORRECT, \
    CONF_MAKE_ID, CONF_NAME, CONF_OUTPUT
from esphome.cpp_generator import get_variable, variable
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App

PLATFORM_SCHEMA = cv.nameable(light.LIGHT_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(light.MakeLight),
    vol.Required(CONF_OUTPUT): cv.use_variable_id(output.FloatOutput),
    vol.Optional(CONF_GAMMA_CORRECT): cv.positive_float,
    vol.Optional(CONF_DEFAULT_TRANSITION_LENGTH): cv.positive_time_period_milliseconds,
    vol.Optional(CONF_EFFECTS): light.validate_effects(light.MONOCHROMATIC_EFFECTS),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for output_ in get_variable(config[CONF_OUTPUT]):
        yield
    rhs = App.make_monochromatic_light(config[CONF_NAME], output_)
    light_struct = variable(config[CONF_MAKE_ID], rhs)
    light.setup_light(light_struct.Pstate, config)
    setup_component(light_struct.Pstate, config)


def to_hass_config(data, config):
    return light.core_to_hass_config(data, config, brightness=True, rgb=False, color_temp=False,
                                     white_value=False)
