import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME
from esphomeyaml.helpers import App, Application, process_lambda, variable

PLATFORM_SCHEMA = binary_sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('template_binary_sensor', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_LAMBDA): cv.lambda_,
}).extend(binary_sensor.BINARY_SENSOR_SCHEMA.schema)

MakeTemplateBinarySensor = Application.MakeTemplateBinarySensor


def to_code(config):
    template_ = process_lambda(config[CONF_LAMBDA], [])
    rhs = App.make_template_binary_sensor(config[CONF_NAME], template_)
    make = variable(MakeTemplateBinarySensor, config[CONF_MAKE_ID], rhs)
    binary_sensor.setup_binary_sensor(make.Ptemplate_, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_TEMPLATE_BINARY_SENSOR'
