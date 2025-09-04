import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID

from .. import (
    CONF_WAVESHARE_IO_CH32V003_ID,
    WaveshareIOCH32V003Component,
    waveshare_io_ch32v003_ns,
)

DEPENDENCIES = ["waveshare_io_ch32v003"]

WaveshareIOCH32V003Output = waveshare_io_ch32v003_ns.class_(
    "WaveshareIOCH32V003Output",
    output.FloatOutput,
    cg.Parented.template(WaveshareIOCH32V003Component),
)

CONF_SAFE_PWM_MIN_VALUE = "safe_pwm_min_value"
CONF_SAFE_PWM_MAX_VALUE = "safe_pwm_max_value"


def validate_pwm_limits(config):
    """Validate that pwm_min_value <= pwm_max_value"""
    min_val = config.get(CONF_SAFE_PWM_MIN_VALUE, 0)
    max_val = config.get(CONF_SAFE_PWM_MAX_VALUE, 247)
    if min_val > max_val:
        raise cv.Invalid(
            f"pwm_min_value ({min_val}) cannot be greater than pwm_max_value ({max_val})"
        )
    return config


CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(WaveshareIOCH32V003Output),
            cv.GenerateID(CONF_WAVESHARE_IO_CH32V003_ID): cv.use_id(
                WaveshareIOCH32V003Component
            ),
            cv.Optional(CONF_SAFE_PWM_MIN_VALUE, default=0): cv.int_range(
                min=0, max=255
            ),
            cv.Optional(CONF_SAFE_PWM_MAX_VALUE, default=247): cv.int_range(
                min=0, max=255
            ),
        }
    ),
    validate_pwm_limits,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await output.register_output(var, config)
    await cg.register_parented(var, config[CONF_WAVESHARE_IO_CH32V003_ID])

    cg.add(
        var.set_pwm_safe_range(
            config[CONF_SAFE_PWM_MIN_VALUE], config[CONF_SAFE_PWM_MAX_VALUE]
        )
    )
