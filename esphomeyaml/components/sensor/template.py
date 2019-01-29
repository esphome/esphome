import voluptuous as vol

from esphomeyaml.components import sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.cpp_generator import Pvariable, add, process_lambda
from esphomeyaml.cpp_helpers import setup_component
from esphomeyaml.cpp_types import App, float_, optional

TemplateSensor = sensor.sensor_ns.class_('TemplateSensor', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateSensor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_template_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    template = Pvariable(config[CONF_ID], rhs)

    sensor.setup_sensor(template, config)
    setup_component(template, config)

    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=optional.template(float_)):
        yield
    add(template.set_template(template_))


BUILD_FLAGS = '-DUSE_TEMPLATE_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
