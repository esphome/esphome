import voluptuous as vol

from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ECHO_PIN, CONF_ID, CONF_NAME, CONF_TIMEOUT_METER, \
    CONF_TIMEOUT_TIME, CONF_TRIGGER_PIN, CONF_UPDATE_INTERVAL
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_input_pin_expression, gpio_output_pin_expression, \
    setup_component
from esphome.cpp_types import App

UltrasonicSensorComponent = sensor.sensor_ns.class_('UltrasonicSensorComponent',
                                                    sensor.PollingSensorComponent)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(UltrasonicSensorComponent),
    vol.Required(CONF_TRIGGER_PIN): pins.gpio_output_pin_schema,
    vol.Required(CONF_ECHO_PIN): pins.internal_gpio_input_pin_schema,
    vol.Exclusive(CONF_TIMEOUT_METER, 'timeout'): cv.positive_float,
    vol.Exclusive(CONF_TIMEOUT_TIME, 'timeout'): cv.positive_time_period_microseconds,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,
}))


def to_code(config):
    for trigger in gpio_output_pin_expression(config[CONF_TRIGGER_PIN]):
        yield
    for echo in gpio_input_pin_expression(config[CONF_ECHO_PIN]):
        yield
    rhs = App.make_ultrasonic_sensor(config[CONF_NAME], trigger, echo,
                                     config.get(CONF_UPDATE_INTERVAL))
    ultrasonic = Pvariable(config[CONF_ID], rhs)

    if CONF_TIMEOUT_TIME in config:
        add(ultrasonic.set_timeout_us(config[CONF_TIMEOUT_TIME]))
    elif CONF_TIMEOUT_METER in config:
        add(ultrasonic.set_timeout_m(config[CONF_TIMEOUT_METER]))

    sensor.setup_sensor(ultrasonic, config)
    setup_component(ultrasonic, config)


BUILD_FLAGS = '-DUSE_ULTRASONIC_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
