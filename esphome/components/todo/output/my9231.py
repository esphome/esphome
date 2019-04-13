from esphome.components import output
from esphome.components.my9231 import MY9231OutputComponent
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_CHANNEL, CONF_ID, CONF_MY9231_ID, CONF_POWER_SUPPLY
DEPENDENCIES = ['my9231']

Channel = MY9231OutputComponent.class_('Channel', output.FloatOutput)

PLATFORM_SCHEMA = output.FLOAT_OUTPUT_PLATFORM_SCHEMA.extend({
    cv.Required(CONF_ID): cv.declare_variable_id(Channel),
    cv.Required(CONF_CHANNEL): cv.All(cv.Coerce(int),
                                        cv.Range(min=0, max=65535)),
    cv.GenerateID(CONF_MY9231_ID): cv.use_variable_id(MY9231OutputComponent),
}).extend(cv.COMPONENT_SCHEMA)


def to_code(config):
    power_supply = None
    if CONF_POWER_SUPPLY in config:
        power_supply = yield get_variable(config[CONF_POWER_SUPPLY])
    my9231 = yield get_variable(config[CONF_MY9231_ID])
    rhs = my9231.create_channel(config[CONF_CHANNEL], power_supply)
    out = Pvariable(config[CONF_ID], rhs)
    output.setup_output_platform(out, config, skip_power_supply=True)
    register_component(out, config)


BUILD_FLAGS = '-DUSE_MY9231_OUTPUT'
