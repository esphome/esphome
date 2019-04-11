import voluptuous as vol

from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ECHO_PIN, CONF_ID, CONF_NAME, CONF_TRIGGER_PIN, \
    CONF_UPDATE_INTERVAL, CONF_TIMEOUT
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_input_pin_expression, gpio_output_pin_expression, \
    register_component
from esphome.cpp_types import App

CONF_PULSE_TIME = 'pulse_time'

UltrasonicSensorComponent = sensor.sensor_ns.class_('UltrasonicSensorComponent',
                                                    sensor.PollingSensorComponent)


PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(UltrasonicSensorComponent),
    vol.Required(CONF_TRIGGER_PIN): pins.gpio_output_pin_schema,
    vol.Required(CONF_ECHO_PIN): pins.internal_gpio_input_pin_schema,

    vol.Optional(CONF_TIMEOUT): cv.distance,
    vol.Optional(CONF_PULSE_TIME): cv.positive_time_period_microseconds,
    vol.Optional(CONF_UPDATE_INTERVAL): cv.update_interval,

    vol.Optional('timeout_meter'): cv.invalid("The timeout_meter option has been renamed "
                                              "to 'timeout'."),
    vol.Optional('timeout_time'): cv.invalid("The timeout_time option has been removed. Please "
                                             "use 'timeout'."),
}))


def to_code(config):
    trigger = yield gpio_output_pin_expression(config[CONF_TRIGGER_PIN])
    echo = yield gpio_input_pin_expression(config[CONF_ECHO_PIN])
    rhs = App.make_ultrasonic_sensor(config[CONF_NAME], trigger, echo,
                                     config.get(CONF_UPDATE_INTERVAL))
    ultrasonic = Pvariable(config[CONF_ID], rhs)

    if CONF_TIMEOUT in config:
        add(ultrasonic.set_timeout_us(config[CONF_TIMEOUT] / (0.000343 / 2)))

    if CONF_PULSE_TIME in config:
        add(ultrasonic.set_pulse_time_us(config[CONF_PULSE_TIME]))

    sensor.setup_sensor(ultrasonic, config)
    register_component(ultrasonic, config)


BUILD_FLAGS = '-DUSE_ULTRASONIC_SENSOR'
