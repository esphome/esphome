import voluptuous as vol

from esphomeyaml.components import text_sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, optional, process_lambda, std_string, \
    variable, setup_component, PollingComponent

MakeTemplateTextSensor = Application.struct('MakeTemplateTextSensor')
TemplateTextSensor = text_sensor.text_sensor_ns.class_('TemplateTextSensor',
                                                       text_sensor.TextSensor, PollingComponent)

PLATFORM_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateTextSensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTemplateTextSensor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_template_text_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    template = make.Ptemplate_
    text_sensor.setup_text_sensor(template, make.Pmqtt, config)
    setup_component(template, config)

    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=optional.template(std_string)):
        yield
    add(template.set_template(template_))


BUILD_FLAGS = '-DUSE_TEMPLATE_TEXT_SENSOR'


def to_hass_config(data, config):
    return text_sensor.core_to_hass_config(data, config)
