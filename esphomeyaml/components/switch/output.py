import voluptuous as vol

from esphomeyaml.components import output, switch
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_NAME, CONF_OUTPUT
from esphomeyaml.cpp_generator import Pvariable, get_variable
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import App, Component

OutputSwitch = switch.switch_ns.class_('OutputSwitch', switch.Switch, Component)

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(OutputSwitch),
    vol.Required(CONF_OUTPUT): cv.use_variable_id(output.BinaryOutput),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for output_ in get_variable(config[CONF_OUTPUT]):
        yield
    rhs = App.make_output_switch(config[CONF_NAME], output_)
    switch_ = Pvariable(config[CONF_ID], rhs)

    switch.setup_switch(switch_, config)
    setup_component(switch, config)


BUILD_FLAGS = '-DUSE_OUTPUT_SWITCH'


def to_hass_config(data, config):
    return switch.core_to_hass_config(data, config)
