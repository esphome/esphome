from esphome import pins
from esphome.components import sensor
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NAME, CONF_RESOLUTION, CONF_MIN_VALUE, CONF_MAX_VALUE


RotaryEncoderResolution = sensor.sensor_ns.enum('RotaryEncoderResolution')
RESOLUTIONS = {
    1: RotaryEncoderResolution.ROTARY_ENCODER_1_PULSE_PER_CYCLE,
    2: RotaryEncoderResolution.ROTARY_ENCODER_2_PULSES_PER_CYCLE,
    4: RotaryEncoderResolution.ROTARY_ENCODER_4_PULSES_PER_CYCLE,
}

CONF_PIN_A = 'pin_a'
CONF_PIN_B = 'pin_b'
CONF_PIN_RESET = 'pin_reset'

RotaryEncoderSensor = sensor.sensor_ns.class_('RotaryEncoderSensor', sensor.Sensor, Component)


def validate_min_max_value(config):
    if CONF_MIN_VALUE in config and CONF_MAX_VALUE in config:
        min_val = config[CONF_MIN_VALUE]
        max_val = config[CONF_MAX_VALUE]
        if min_val >= max_val:
            raise cv.Invalid("Max value {} must be smaller than min value {}"
                              "".format(max_val, min_val))
    return config


PLATFORM_SCHEMA = cv.nameable(sensor.SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(RotaryEncoderSensor),
    cv.Required(CONF_PIN_A): cv.All(pins.internal_gpio_input_pin_schema,
                                      pins.validate_has_interrupt),
    cv.Required(CONF_PIN_B): cv.All(pins.internal_gpio_input_pin_schema,
                                      pins.validate_has_interrupt),
    cv.Optional(CONF_PIN_RESET): pins.internal_gpio_input_pin_schema,
    cv.Optional(CONF_RESOLUTION): cv.one_of(*RESOLUTIONS, int=True),
    cv.Optional(CONF_MIN_VALUE): cv.int_,
    cv.Optional(CONF_MAX_VALUE): cv.int_,
}).extend(cv.COMPONENT_SCHEMA), validate_min_max_value)


def to_code(config):
    pin_a = yield gpio_input_pin_expression(config[CONF_PIN_A])
    pin_b = yield gpio_input_pin_expression(config[CONF_PIN_B])
    rhs = App.make_rotary_encoder_sensor(config[CONF_NAME], pin_a, pin_b)
    encoder = Pvariable(config[CONF_ID], rhs)

    if CONF_PIN_RESET in config:
        pin_i = yield gpio_input_pin_expression(config[CONF_PIN_RESET])
        cg.add(encoder.set_reset_pin(pin_i))
    if CONF_RESOLUTION in config:
        resolution = RESOLUTIONS[config[CONF_RESOLUTION]]
        cg.add(encoder.set_resolution(resolution))
    if CONF_MIN_VALUE in config:
        cg.add(encoder.set_min_value(config[CONF_MIN_VALUE]))
    if CONF_MAX_VALUE in config:
        cg.add(encoder.set_max_value(config[CONF_MAX_VALUE]))

    sensor.setup_sensor(encoder, config)
    register_component(encoder, config)


BUILD_FLAGS = '-DUSE_ROTARY_ENCODER_SENSOR'
