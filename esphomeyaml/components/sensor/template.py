import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, process_lambda, variable, Application

MakeTemplateSensor = Application.MakeTemplateSensor

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTemplateSensor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(sensor.SENSOR_SCHEMA.schema)


def to_code(config):
    template_ = None
    for template_ in process_lambda(config[CONF_LAMBDA], []):
        yield
    rhs = App.make_template_sensor(config[CONF_NAME], template_,
                                   config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    for _ in sensor.setup_sensor(make.Ptemplate_, make.Pmqtt, config):
        yield


BUILD_FLAGS = '-DUSE_TEMPLATE_SENSOR'
