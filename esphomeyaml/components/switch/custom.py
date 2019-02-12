import voluptuous as vol

from esphomeyaml.components import switch
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_LAMBDA, CONF_SWITCHES, CONF_NAME
from esphomeyaml.cpp_generator import process_lambda, variable, Pvariable, add
from esphomeyaml.cpp_types import std_vector

CustomSwitchConstructor = switch.switch_ns.class_('CustomSwitchConstructor')

PLATFORM_SCHEMA = switch.PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(CustomSwitchConstructor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Required(CONF_SWITCHES):
        cv.ensure_list(switch.SWITCH_SCHEMA.extend({
            cv.GenerateID(): cv.declare_variable_id(switch.Switch),
        })),
})


def to_code(config):
    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=std_vector.template(switch.SwitchPtr)):
        yield

    rhs = CustomSwitchConstructor(template_)
    custom = variable(config[CONF_ID], rhs)
    for i, conf in enumerate(config[CONF_SWITCHES]):
        var = Pvariable(conf[CONF_ID], custom.get_switch(i))
        add(var.set_name(conf[CONF_NAME]))
        switch.setup_switch(var, conf)


BUILD_FLAGS = '-DUSE_CUSTOM_SWITCH'


def to_hass_config(data, config):
    return [switch.core_to_hass_config(data, swi) for swi in config[CONF_SWITCHES]]
