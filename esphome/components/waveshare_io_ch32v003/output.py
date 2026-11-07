from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_MAX_VALUE, CONF_MIN_VALUE
from esphome.types import ConfigType

from . import CONF_WAVESHARE_IO_CH32V003_ID, WaveshareIOCH32V003Component

DEPENDENCIES = ["waveshare_io_ch32v003"]


CONF_SAFE_PWM_LEVELS = "safe_pwm_levels"

DUTY_DEFAULT_MIN = 1
DUTY_DEFAULT_MAX = 247


def validate_pwm_limits(config: ConfigType) -> ConfigType:
    """Validate that safe_pwm_levels.min_value <= safe_pwm_levels.max_value."""

    min_val = config.get(CONF_SAFE_PWM_LEVELS, {}).get(CONF_MIN_VALUE, DUTY_DEFAULT_MIN)
    max_val = config.get(CONF_SAFE_PWM_LEVELS, {}).get(CONF_MAX_VALUE, DUTY_DEFAULT_MAX)
    if min_val > max_val:
        raise cv.Invalid(
            f"safe_pwm_levels.min_value ({min_val}) cannot be greater than "
            f"safe_pwm_levels.max_value ({max_val})"
        )
    return config


CONF_SAFE_PWM_LEVELS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MIN_VALUE, default=DUTY_DEFAULT_MIN): cv.uint8_t,
        cv.Optional(CONF_MAX_VALUE, default=DUTY_DEFAULT_MAX): cv.uint8_t,
    }
)

CONFIG_SCHEMA = cv.All(
    output.FLOAT_OUTPUT_SCHEMA.extend(
        {
            cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
            cv.GenerateID(CONF_WAVESHARE_IO_CH32V003_ID): cv.use_id(
                WaveshareIOCH32V003Component
            ),
            cv.Optional(CONF_SAFE_PWM_LEVELS): CONF_SAFE_PWM_LEVELS_SCHEMA,
        }
    ),
    validate_pwm_limits,
)


async def to_code(config: ConfigType) -> None:
    min_val = config.get(CONF_SAFE_PWM_LEVELS, {}).get(CONF_MIN_VALUE, DUTY_DEFAULT_MIN)
    max_val = config.get(CONF_SAFE_PWM_LEVELS, {}).get(CONF_MAX_VALUE, DUTY_DEFAULT_MAX)
    await output.sub_float_outputs(
        config, parent=config[CONF_WAVESHARE_IO_CH32V003_ID]
    ).create(
        config,
        None,
        apply=f"write_pwm_level(state, {min_val}, {max_val})",
    )
