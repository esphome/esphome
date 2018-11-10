import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import switch, output
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_OUTPUT
from esphomeyaml.helpers import App, Application, get_variable, variable, setup_component, Component

MakeOutputSwitch = Application.struct('MakeOutputSwitch')
OutputSwitch = switch.switch_ns.class_('OutputSwitch', switch.Switch, Component)

PLATFORM_SCHEMA = cv.nameable(switch.SWITCH_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(OutputSwitch),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeOutputSwitch),
    vol.Required(CONF_OUTPUT): cv.use_variable_id(output.BinaryOutput),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for output in get_variable(config[CONF_OUTPUT]):
        yield
    rhs = App.make_output_switch(config[CONF_NAME], output)
    make = variable(config[CONF_MAKE_ID], rhs)
    switch_ = make.Pswitch_
    
    switch.setup_switch(switch_, make.Pmqtt, config)
    setup_component(switch, config)


BUILD_FLAGS = '-DUSE_OUTPUT_SWITCH'


def to_hass_config(data, config):
    return switch.core_to_hass_config(data, config)
