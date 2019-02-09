import voluptuous as vol

from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_PIN, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable
from esphome.cpp_helpers import gpio_input_pin_expression, setup_component
from esphome.cpp_types import App

DutyCycleSensor = sensor.sensor_ns.class_('DutyCycleSensor', sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(DutyCycleSensor),
    vol.Required(CONF_PIN): pins.internal_gpio_input_pin_schema,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for pin in gpio_input_pin_expression(config[CONF_PIN]):
        yield
    rhs = App.make_duty_cycle_sensor(config[CONF_NAME], pin,
                                     config.get(CONF_UPDATE_INTERVAL))
    duty = Pvariable(config[CONF_ID], rhs)
    sensor.setup_sensor(duty, config)
    setup_component(duty, config)


BUILD_FLAGS = '-DUSE_DUTY_CYCLE_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
