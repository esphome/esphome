import voluptuous as vol

from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_RESOLUTION
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_helpers import gpio_input_pin_expression, setup_component
from esphome.cpp_types import App, Component

RotaryEncoderResolution = sensor.sensor_ns.enum('RotaryEncoderResolution')
RESOLUTIONS = {
    '1': RotaryEncoderResolution.ROTARY_ENCODER_1_PULSE_PER_CYCLE,
    '2': RotaryEncoderResolution.ROTARY_ENCODER_2_PULSES_PER_CYCLE,
    '4': RotaryEncoderResolution.ROTARY_ENCODER_4_PULSES_PER_CYCLE,
}

CONF_PIN_A = 'pin_a'
CONF_PIN_B = 'pin_b'
CONF_PIN_RESET = 'pin_reset'

RotaryEncoderSensor = sensor.sensor_ns.class_('RotaryEncoderSensor', sensor.Sensor, Component)

PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(RotaryEncoderSensor),
    vol.Required(CONF_PIN_A): pins.internal_gpio_input_pin_schema,
    vol.Required(CONF_PIN_B): pins.internal_gpio_input_pin_schema,
    vol.Optional(CONF_PIN_RESET): pins.internal_gpio_input_pin_schema,
    vol.Optional(CONF_RESOLUTION): cv.one_of(*RESOLUTIONS, string=True),
}).extend(cv.COMPONENT_SCHEMA.schema))


def to_code(config):
    for pin_a in gpio_input_pin_expression(config[CONF_PIN_A]):
        yield
    for pin_b in gpio_input_pin_expression(config[CONF_PIN_B]):
        yield
    rhs = App.make_rotary_encoder_sensor(config[CONF_NAME], pin_a, pin_b)
    encoder = Pvariable(config[CONF_ID], rhs)

    if CONF_PIN_RESET in config:
        for pin_i in gpio_input_pin_expression(config[CONF_PIN_RESET]):
            yield
        add(encoder.set_reset_pin(pin_i))
    if CONF_RESOLUTION in config:
        resolution = RESOLUTIONS[config[CONF_RESOLUTION]]
        add(encoder.set_resolution(resolution))

    sensor.setup_sensor(encoder, config)
    setup_component(encoder, config)


BUILD_FLAGS = '-DUSE_ROTARY_ENCODER_SENSOR'


def to_hass_config(data, config):
    return sensor.core_to_hass_config(data, config)
