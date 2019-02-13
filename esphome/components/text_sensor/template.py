import voluptuous as vol

from esphome.components import text_sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA, CONF_NAME, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add, process_lambda
from esphome.cpp_helpers import setup_component
from esphome.cpp_types import App, PollingComponent, optional, std_string

TemplateTextSensor = text_sensor.text_sensor_ns.class_('TemplateTextSensor',
                                                       text_sensor.TextSensor, PollingComponent)

PLATFORM_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateTextSensor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_template_text_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    template = Pvariable(config[CONF_ID], rhs)
    text_sensor.setup_text_sensor(template, config)
    setup_component(template, config)

    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=optional.template(std_string)):
        yield
    add(template.set_template(template_))


BUILD_FLAGS = '-DUSE_TEMPLATE_TEXT_SENSOR'


def to_hass_config(data, config):
    return text_sensor.core_to_hass_config(data, config)
