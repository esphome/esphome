import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml import pins
from esphomeyaml.components import sensor
from esphomeyaml.const import CONF_MAKE_ID, CONF_NAME, CONF_RESOLUTION
from esphomeyaml.helpers import App, Application, add, gpio_input_pin_expression, variable

RESOLUTIONS = {
    '1': sensor.sensor_ns.ROTARY_ENCODER_1_PULSE_PER_CYCLE,
    '2': sensor.sensor_ns.ROTARY_ENCODER_2_PULSES_PER_CYCLE,
    '4': sensor.sensor_ns.ROTARY_ENCODER_4_PULSES_PER_CYCLE,
}

CONF_PIN_A = 'pin_a'
CONF_PIN_B = 'pin_b'
CONF_PIN_RESET = 'pin_reset'

PLATFORM_SCHEMA = sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('rotary_encoder', CONF_MAKE_ID): cv.register_variable_id,
    vol.Required(CONF_PIN_A): pins.GPIO_INTERNAL_INPUT_PIN_SCHEMA,
    vol.Required(CONF_PIN_B): pins.GPIO_INTERNAL_INPUT_PIN_SCHEMA,
    vol.Optional(CONF_PIN_RESET): pins.GPIO_INTERNAL_INPUT_PIN_SCHEMA,
    vol.Optional(CONF_RESOLUTION): vol.All(cv.string, cv.one_of(*RESOLUTIONS)),
}).extend(sensor.SENSOR_SCHEMA.schema)

MakeRotaryEncoderSensor = Application.MakeRotaryEncoderSensor


def to_code(config):
    pin_a = gpio_input_pin_expression(config[CONF_PIN_A])
    pin_b = gpio_input_pin_expression(config[CONF_PIN_B])
    rhs = App.make_rotary_encoder_sensor(config[CONF_NAME], pin_a, pin_b)
    make = variable(MakeRotaryEncoderSensor, config[CONF_MAKE_ID], rhs)
    encoder = make.Protary_encoder
    if CONF_PIN_RESET in config:
        pin_i = gpio_input_pin_expression(config[CONF_PIN_RESET])
        add(encoder.set_reset_pin(pin_i))
    if CONF_RESOLUTION in config:
        resolution = RESOLUTIONS[config[CONF_RESOLUTION]]
        add(encoder.set_resolution(resolution))
    sensor.setup_sensor(encoder, make.Pmqtt, config)


BUILD_FLAGS = '-DUSE_ROTARY_ENCODER_SENSOR'
