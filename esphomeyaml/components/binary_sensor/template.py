import voluptuous as vol

from esphomeyaml.components import binary_sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_LAMBDA, CONF_NAME
from esphomeyaml.cpp_generator import Pvariable, add, process_lambda
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import App, Component, bool_, optional

TemplateBinarySensor = binary_sensor.binary_sensor_ns.class_('TemplateBinarySensor',
                                                             binary_sensor.BinarySensor,
                                                             Component)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateBinarySensor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_template_binary_sensor(config[CONF_NAME])
    var = Pvariable(config[CONF_ID], rhs)
    binary_sensor.setup_binary_sensor(var, config)
    setup_component(var, config)

    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=optional.template(bool_)):
        yield
    add(var.set_template(template_))


BUILD_FLAGS = '-DUSE_TEMPLATE_BINARY_SENSOR'


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)
