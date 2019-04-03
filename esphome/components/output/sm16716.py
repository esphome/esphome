import voluptuous as vol

from esphome.components import output
from esphome.components.sm16716 import SM16716OutputComponent
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_SM16716_ID, CONF_POWER_SUPPLY
from esphome.cpp_generator import Pvariable, get_variable
from esphome.cpp_helpers import setup_component

DEPENDENCIES = ['sm16716']

Channel = SM16716OutputComponent.class_('Channel', output.FloatOutput)

PLATFORM_SCHEMA = output.FLOAT_OUTPUT_PLATFORM_SCHEMA.extend({
    vol.Required(CONF_ID): cv.declare_variable_id(Channel),
    vol.Required(CONF_CHANNEL): vol.All(vol.Coerce(int),
                                        vol.Range(min=0, max=65535)),
    cv.GenerateID(CONF_SM16716_ID): cv.use_variable_id(SM16716OutputComponent),
}).extend(cv.COMPONENT_SCHEMA.schema)


def to_code(config):
    power_supply = None
    if CONF_POWER_SUPPLY in config:
        for power_supply in get_variable(config[CONF_POWER_SUPPLY]):
            yield
    sm16716 = None
    for sm16716 in get_variable(config[CONF_SM16716_ID]):
        yield
    rhs = sm16716.create_channel(config[CONF_CHANNEL], power_supply)
    out = Pvariable(config[CONF_ID], rhs)
    output.setup_output_platform(out, config, skip_power_supply=True)
    setup_component(out, config)


BUILD_FLAGS = '-DUSE_SM16716_OUTPUT'
