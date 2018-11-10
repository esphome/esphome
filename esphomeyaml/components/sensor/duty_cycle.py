import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_PIN, CONF_UPDATE_INTERVAL
from esphomeyaml.helpers import App, Application, gpio_input_pin_expression, variable, \
    setup_component

MakeDutyCycleSensor = Application.struct('MakeDutyCycleSensor')
DutyCycleSensor = sensor.sensor_ns.class_('DutyCycleSensor', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(DutyCycleSensor),
    cv.GenerateID(CONF_MAKE_ID): cv.declare_variable_id(MakeDutyCycleSensor),
    vol.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for pin in gpio_input_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_duty_cycle_sensor(config[CONF_NAME], pin,
                                     config.get(CONF_UPDATE_INTERVAL))
    make = variable(config[CONF_MAKE_ID], rhs)
    sensor.setup_sensor(make.Pduty, make.Pmqtt, config)
    setup_component(make.Pduty, config)


BUILD_FLAGS = '-DUSE_DUTY_CYCLE_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
