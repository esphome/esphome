import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.components import sensor
from esphome.const import CONF_ID, CONF_RESOLUTION, CONF_MIN_VALUE, CONF_MAX_VALUE, UNIT_STEPS, \
    ICON_ROTATE_RIGHT, CONF_VALUE, CONF_PIN_A, CONF_PIN_B

rotary_encoder_ns = cg.esphome_ns.namespace('rotary_encoder')
RotaryEncoderResolution = rotary_encoder_ns.enum('RotaryEncoderResolution')
RESOLUTIONS = {
    1: RotaryEncoderResolution.ROTARY_ENCODER_1_PULSE_PER_CYCLE,
    2: RotaryEncoderResolution.ROTARY_ENCODER_2_PULSES_PER_CYCLE,
    4: RotaryEncoderResolution.ROTARY_ENCODER_4_PULSES_PER_CYCLE,
}

CONF_PIN_RESET = 'pin_reset'

RotaryEncoderSensor = rotary_encoder_ns.class_('RotaryEncoderSensor', sensor.Sensor, cg.Component)
RotaryEncoderSetValueAction = rotary_encoder_ns.class_('RotaryEncoderSetValueAction',
                                                       automation.Action)


def validate_min_max_value(config):
    if CONF_MIN_VALUE in config and CONF_MAX_VALUE in config:
        min_val = config[CONF_MIN_VALUE]
        max_val = config[CONF_MAX_VALUE]
        if min_val >= max_val:
            raise cv.Invalid("Max value {} must be smaller than min value {}"
                             "".format(max_val, min_val))
    return config


CONFIG_SCHEMA = cv.All(sensor.sensor_schema(UNIT_STEPS, ICON_ROTATE_RIGHT, 0).extend({
    cv.GenerateID(): cv.declare_id(RotaryEncoderSensor),
    cv.Required(CONF_PIN_A): cv.All(pins.internal_gpio_input_pin_schema,
                                    pins.validate_has_interrupt),
    cv.Required(CONF_PIN_B): cv.All(pins.internal_gpio_input_pin_schema,
                                    pins.validate_has_interrupt),
    cv.Optional(CONF_PIN_RESET): pins.internal_gpio_output_pin_schema,
    cv.Optional(CONF_RESOLUTION, default=1): cv.enum(RESOLUTIONS, int=True),
    cv.Optional(CONF_MIN_VALUE): cv.int_,
    cv.Optional(CONF_MAX_VALUE): cv.int_,
}).extend(cv.COMPONENT_SCHEMA), validate_min_max_value)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield sensor.register_sensor(var, config)
    pin_a = yield cg.gpio_pin_expression(config[CONF_PIN_A])
    cg.add(var.set_pin_a(pin_a))
    pin_b = yield cg.gpio_pin_expression(config[CONF_PIN_B])
    cg.add(var.set_pin_b(pin_b))

    if CONF_PIN_RESET in config:
        pin_i = yield cg.gpio_pin_expression(config[CONF_PIN_RESET])
        cg.add(var.set_reset_pin(pin_i))
    cg.add(var.set_resolution(config[CONF_RESOLUTION]))
    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    if CONF_MAX_VALUE in config:
        cg.add(var.set_max_value(config[CONF_MAX_VALUE]))


@automation.register_action('sensor.rotary_encoder.set_value', RotaryEncoderSetValueAction,
                            cv.Schema({
                                cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
                                cv.Required(CONF_VALUE): cv.templatable(cv.int_),
                            }))
def sensor_template_publish_to_code(config, action_id, template_arg, args):
    paren = yield cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = yield cg.templatable(config[CONF_VALUE], args, int)
    cg.add(var.set_value(template_))
    yield var
