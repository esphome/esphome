import voluptuous as vol

from esphome.components import output
from esphome.components.my9231 import MY9231OutputComponent
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_MY9231_ID, CONF_POWER_SUPPLY
from esphome.cpp_generator import Pvariable, get_variable
from esphome.cpp_helpers import setup_component

DEPENDENCIES = ['my9231']

Channel = MY9231OutputComponent.class_('Channel', output.FloatOutput)

PLATFORM_SCHEMA = output.FLOAT_OUTPUT_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_ID): cv.declare_variable_id(Channel),
    vol.Required(CONF_CHANNEL): vol.All(vol.Coerce(int),
                                        vol.Range(min=0, max=65535)),
    cv.GenerateID(CONF_MY9231_ID): cv.use_variable_id(MY9231OutputComponent),
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    power_supply = None
    if CONF_POWER_SUPPLY in config:
        for power_supply in get_variable(config[CONF_POWER_SUPPLY]):
            yield
    my9231 = None
    for my9231 in get_variable(config[CONF_MY9231_ID]):
        yield
    rhs = my9231.create_channel(config[CONF_CHANNEL], power_supply)
    out = Pvariable(config[CONF_ID], rhs)
    output.setup_output_platform(out, config, skip_power_supply=True)
    setup_component(out, config)


BUILD_FLAGS = '-DUSE_MY9231_OUTPUT'
