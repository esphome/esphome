import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, process_lambda, variable, Application, float_, optional, add

MakeTemplateSensor = Application.MakeTemplateSensor

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTemplateSensor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}))


def to_code(config):
    rhs = App.make_template_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    sensor.setup_sensor(make.Ptemplate_, make.Pmqtt, config)

    template_ = None
    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=optional.template(float_)):
        yield
    add(make.Ptemplate_.set_template(template_))


BUILD_FLAGS = '-DUSE_TEMPLATE_SENSOR'
