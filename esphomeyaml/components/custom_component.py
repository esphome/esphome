import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_LAMBDA
from esphomeyaml.cpp_generator import process_lambda, variable
from esphomeyaml.cpp_types import Component, ComponentPtr, esphomelib_ns, std_vector

CustomComponentConstructor = esphomelib_ns.class_('CustomComponentConstructor')

CUSTOM_COMPONENT_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(Component),
    vol.Required(CONF_LAMBDA): cv.lambda_,
})


def to_code(config):
    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=std_vector.template(ComponentPtr)):
        yield

    rhs = CustomComponentConstructor(template_)
    variable(config[CONF_ID], rhs)


BUILD_FLAGS = '-DUSE_CUSTOM_COMPONENT'
