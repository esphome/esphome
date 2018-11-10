import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_LAMBDA, CONF_MAKE_ID, CONF_NAME, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, process_lambda, variable, Application, float_, optional, add, \
    setup_component

MakeTemplateSensor = Application.struct('MakeTemplateSensor')
TemplateSensor = sensor.sensor_ns.class_('TemplateSensor', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(TemplateSensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeTemplateSensor),
    vol.Required(CONF_LAMBDA): cv.lambda_,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    rhs = App.make_template_sensor(config[CONF_NAME], config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    template = make.Ptemplate_

    sensor.setup_sensor(template, make.Pmqtt, config)
    setup_component(template, config)

    for template_ in process_lambda(config[CONF_LAMBDA], [],
                                    return_type=optional.template(float_)):
        yield
    add(template.set_template(template_))


BUILD_FLAGS = '-DUSE_TEMPLATE_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
