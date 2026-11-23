from esphome import pins
import esphome.codegen as cg
from esphome.components import stepper
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_PIN_A,
    CONF_PIN_B,
    CONF_PIN_C,
    CONF_PIN_D,
    CONF_SLEEP_WHEN_DONE,
    CONF_STEP_MODE,
)
from esphome.types import ConfigType

uln2003_ns = cg.esphome_ns.namespace("uln2003")
ULN2003StepMode = uln2003_ns.enum("ULN2003StepMode")

STEP_MODES = ULN2003StepMode.options(
    "FULL_STEP", "HALF_STEP", "WAVE_DRIVE", prefix="ULN2003_STEP_MODE_"
)

ULN2003 = uln2003_ns.class_("ULN2003", stepper.Stepper, cg.Component)

CONFIG_SCHEMA = stepper.STEPPER_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(ULN2003),
        cv.Required(CONF_PIN_A): pins.gpio_output_pin_schema,
        cv.Required(CONF_PIN_B): pins.gpio_output_pin_schema,
        cv.Required(CONF_PIN_C): pins.gpio_output_pin_schema,
        cv.Required(CONF_PIN_D): pins.gpio_output_pin_schema,
        cv.Optional(CONF_SLEEP_WHEN_DONE, default=False): cv.boolean,
        cv.Optional(CONF_STEP_MODE, default="FULL_STEP"): cv.enum(
            STEP_MODES, upper=True, space="_"
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await stepper.register_stepper(var, config)

    await cg.add_pin(var.set_pin_a, config[CONF_PIN_A])
    await cg.add_pin(var.set_pin_b, config[CONF_PIN_B])
    await cg.add_pin(var.set_pin_c, config[CONF_PIN_C])
    await cg.add_pin(var.set_pin_d, config[CONF_PIN_D])

    cg.add(var.set_sleep_when_done(config[CONF_SLEEP_WHEN_DONE]))
    cg.add(var.set_step_mode(config[CONF_STEP_MODE]))
