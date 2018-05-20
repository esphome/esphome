import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, process_lambda, variable, Application

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('template_sensor', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.positive_time_period_milliseconds,
}).extend(sensor.SENSOR_SCHEMA.schema)

MakeTemplateSensor = Application.MakeTemplateSensor


def to_code(config):
    template_ = process_lambda(config[CONF_LAMBDA], [])
    rhs = App.make_template_sensor(config[CONF_NAME], template_,
                                   config.get(CONF_UPDATE_INTERVAL))
    make = variable(MakeTemplateSensor, config[CONF_MAKE_ID], rhs)
    sensor.setup_sensor(make.Ptemplate_, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_TEMPLATE_SENSOR'
