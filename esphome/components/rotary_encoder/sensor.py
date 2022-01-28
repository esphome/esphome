import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins, automation
from esphome.components import sensor
from esphome.const import (
    CONF_ID,
    CONF_RESOLUTION,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    STATE_CLASS_NONE,
    UNIT_STEPS,
    ICON_ROTATE_RIGHT,
    CONF_VALUE,
    CONF_PIN_A,
    CONF_PIN_B,
    CONF_TRIGGER_ID,
    CONF_RESTORE_MODE,
)

rotary_encoder_ns = cg.esphome_ns.namespace("rotary_encoder")

RotaryEncoderRestoreMode = rotary_encoder_ns.enum("RotaryEncoderRestoreMode")
RESTORE_MODES = {
    "RESTORE_DEFAULT_ZERO": RotaryEncoderRestoreMode.ROTARY_ENCODER_RESTORE_DEFAULT_ZERO,
    "ALWAYS_ZERO": RotaryEncoderRestoreMode.ROTARY_ENCODER_ALWAYS_ZERO,
}

RotaryEncoderResolution = rotary_encoder_ns.enum("RotaryEncoderResolution")
RESOLUTIONS = {
    1: RotaryEncoderResolution.ROTARY_ENCODER_1_PULSE_PER_CYCLE,
    2: RotaryEncoderResolution.ROTARY_ENCODER_2_PULSES_PER_CYCLE,
    4: RotaryEncoderResolution.ROTARY_ENCODER_4_PULSES_PER_CYCLE,
}

CONF_PIN_RESET = "pin_reset"
CONF_ON_CLOCKWISE = "on_clockwise"
CONF_ON_ANTICLOCKWISE = "on_anticlockwise"
CONF_PUBLISH_INITIAL_VALUE = "publish_initial_value"

RotaryEncoderSensor = rotary_encoder_ns.class_(
    "RotaryEncoderSensor", sensor.Sensor, cg.Component
)
RotaryEncoderSetValueAction = rotary_encoder_ns.class_(
    "RotaryEncoderSetValueAction", automation.Action
)

RotaryEncoderClockwiseTrigger = rotary_encoder_ns.class_(
    "RotaryEncoderClockwiseTrigger", automation.Trigger
)
RotaryEncoderAnticlockwiseTrigger = rotary_encoder_ns.class_(
    "RotaryEncoderAnticlockwiseTrigger", automation.Trigger
)


def validate_min_max_value(config):
    if CONF_MIN_VALUE in config and CONF_MAX_VALUE in config:
        min_val = config[CONF_MIN_VALUE]
        max_val = config[CONF_MAX_VALUE]
        if min_val >= max_val:
            raise cv.Invalid(
                f"Max value {max_val} must be smaller than min value {min_val}"
            )
    return config


CONFIG_SCHEMA = cv.All(
    sensor.sensor_schema(
        unit_of_measurement=UNIT_STEPS,
        icon=ICON_ROTATE_RIGHT,
        accuracy_decimals=0,
        state_class=STATE_CLASS_NONE,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(RotaryEncoderSensor),
            cv.Required(CONF_PIN_A): cv.All(pins.internal_gpio_input_pin_schema),
            cv.Required(CONF_PIN_B): cv.All(pins.internal_gpio_input_pin_schema),
            cv.Optional(CONF_PIN_RESET): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_RESOLUTION, default=1): cv.enum(RESOLUTIONS, int=True),
            cv.Optional(CONF_MIN_VALUE): cv.int_,
            cv.Optional(CONF_MAX_VALUE): cv.int_,
            cv.Optional(CONF_PUBLISH_INITIAL_VALUE, default=False): cv.boolean,
            cv.Optional(CONF_RESTORE_MODE, default="RESTORE_DEFAULT_ZERO"): cv.enum(
                RESTORE_MODES, upper=True, space="_"
            ),
            cv.Optional(CONF_ON_CLOCKWISE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        RotaryEncoderClockwiseTrigger
                    ),
                }
            ),
            cv.Optional(CONF_ON_ANTICLOCKWISE): automation.validate_automation(
                {
                    cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                        RotaryEncoderAnticlockwiseTrigger
                    ),
                }
            ),
        }
    )
    .extend(cv.COMPONENT_SCHEMA),
    validate_min_max_value,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    pin_a = await cg.gpio_pin_expression(config[CONF_PIN_A])
    cg.add(var.set_pin_a(pin_a))
    pin_b = await cg.gpio_pin_expression(config[CONF_PIN_B])
    cg.add(var.set_pin_b(pin_b))
    cg.add(var.set_publish_initial_value(config[CONF_PUBLISH_INITIAL_VALUE]))
    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))

    if CONF_PIN_RESET in config:
        pin_i = await cg.gpio_pin_expression(config[CONF_PIN_RESET])
        cg.add(var.set_reset_pin(pin_i))
    cg.add(var.set_resolution(config[CONF_RESOLUTION]))
    if CONF_MIN_VALUE in config:
        cg.add(var.set_min_value(config[CONF_MIN_VALUE]))
    if CONF_MAX_VALUE in config:
        cg.add(var.set_max_value(config[CONF_MAX_VALUE]))

    for conf in config.get(CONF_ON_CLOCKWISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)
    for conf in config.get(CONF_ON_ANTICLOCKWISE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)


@automation.register_action(
    "sensor.rotary_encoder.set_value",
    RotaryEncoderSetValueAction,
    cv.Schema(
        {
            cv.Required(CONF_ID): cv.use_id(sensor.Sensor),
            cv.Required(CONF_VALUE): cv.templatable(cv.int_),
        }
    ),
)
async def sensor_template_publish_to_code(config, action_id, template_arg, args):
    paren = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, paren)
    template_ = await cg.templatable(config[CONF_VALUE], args, int)
    cg.add(var.set_value(template_))
    return var
