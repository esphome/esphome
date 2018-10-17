import voluptuous as vol

from esphomeyaml.components import text_sensor
import esphomeyaml.config_validation as cv
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, add, optional, process_lambda, std_string, \
    variable

MakeTemplateTextSensor = Application.MakeTemplateTextSensor

PLATFORM_SCHEMA = cv.nameable(text_sensor.TEXT_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTemplateTextSensor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}))


def to_code(config):
    rhs = App.make_template_text_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    text_sensor.setup_text_sensor(make.Ptemplate_, make.Pmqtt, config)

    template_ = None
    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=optional.template(std_string)):
        yield
    add(make.Ptemplate_.set_template(template_))


BUILD_FLAGS = '-DUSE_TEMPLATE_TEXT_SENSOR'
