import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_LAMBDA, CONF_COMPONENTS
from esphomeyaml.cpp_generator import process_lambda, variable
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import Component, ComponentPtr, esphomelib_ns, std_vector

CustomComponentConstructor = esphomelib_ns.class_('CustomComponentConstructor')

CUSTOM_COMPONENT_SCHEMA = vol.Schema({
    cv.GenerateID(): cv.declare_variable_id(CustomComponentConstructor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_COMPONENTS): vol.All(cv.ensure_list, [vol.Schema({
        cv.GenerateID(): cv.declare_variable_id(Component)
    }).extend(cv.COMPONENT_SCHEMA.schema)]),
})

CONFIG_SCHEMA = vol.All(cv.ensure_list, [CUSTOM_COMPONENT_SCHEMA])


def to_code(config):
    for conf in config:
        for template_ in process_lambda(conf[CONF_LAMBDA], [],
                                        return_type=std_vector.template(ComponentPtr)):
            yield

        rhs = CustomComponentConstructor(template_)
        custom = variable(conf[CONF_ID], rhs)
        for i, comp in enumerate(conf.get(CONF_COMPONENTS, [])):
            setup_component(custom.get_component(i), comp)


BUILD_FLAGS = '-DUSE_CUSTOM_COMPONENT'
