from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
import esphome.config_validation as cv
from esphome.const import CONF_DIRECTION, CONF_HYSTERESIS, CONF_ID, CONF_MODE
from esphome.types import ConfigType

from ..mt6701 import MT6701Component

CODEOWNERS = ["@slimcdk"]
AUTO_LOAD = ["mt6701"]
DEPENDENCIES = ["i2c"]
MULTI_CONF = True

mt6701_i2c_ns = cg.esphome_ns.namespace("mt6701_i2c")
MT6701I2CComponent = mt6701_i2c_ns.class_(
    "MT6701I2CComponent", MT6701Component, i2c.I2CDevice
)
SaveEEPROMAction = mt6701_i2c_ns.class_("SaveEEPROMAction", automation.Action)

CONF_ZERO_OFFSET = "zero_offset"
CONF_OUTPUT_MODE = "output_mode"
CONF_ABZ = "abz"
CONF_PULSES_PER_REVOLUTION = "pulses_per_revolution"
CONF_Z_PULSE_WIDTH = "z_pulse_width"
CONF_UVW = "uvw"
CONF_POLE_PAIRS = "pole_pairs"
CONF_OUT_PIN = "out_pin"
CONF_PWM_FREQUENCY = "pwm_frequency"
CONF_PWM_POLARITY = "pwm_polarity"
CONF_ANALOG_START = "analog_start"
CONF_ANALOG_STOP = "analog_stop"

# Register bit value written for each rotation direction (DIR bit, datasheet
# section 8.1: DIR=1 means the angle increases clockwise).
DIRECTION = {
    "CLOCKWISE": 1,
    "COUNTERCLOCKWISE": 0,
}

# Hysteresis expressed in output LSBs -> HYST register code.
HYSTERESIS = {
    0: 4,
    0.25: 5,
    0.5: 6,
    1: 0,
    2: 1,
    4: 2,
    8: 3,
}

Z_PULSE_WIDTH = {
    "1LSB": 0,
    "2LSB": 1,
    "4LSB": 2,
    "8LSB": 3,
    "12LSB": 4,
    "16LSB": 5,
    "180DEG": 6,
}

OUTPUT_MODE = {
    "ABZ": 0,
    "UVW": 1,
}

OUT_PIN_MODE = {
    "ANALOG": 0,
    "PWM": 1,
}

PWM_FREQUENCY = {
    "994.4HZ": 0,
    "497.2HZ": 1,
}

PWM_POLARITY = {
    "HIGH": 0,
    "LOW": 1,
}

# The configurable position registers (zero offset, analog start/stop) are
# 12-bit values covering one full revolution.
RESOLUTION_12BIT = 4096
MAX_POSITION_12BIT = RESOLUTION_12BIT - 1
ANGLE_TO_POSITION_12 = RESOLUTION_12BIT / 360


def _angle_to_position_12(value: Any) -> int:
    value = cv.float_range(min=0, max=360)(
        cv.float_with_unit("angle", "(°|deg)")(value)
    )
    # The register cannot represent a full 360°, so clamp the top of the range
    # to the maximum count instead of letting it wrap back to 0.
    return min(round(value * ANGLE_TO_POSITION_12), MAX_POSITION_12BIT)


def _percent_to_position_12(value: Any) -> int:
    value = cv.percentage(value)
    return min(round(value * RESOLUTION_12BIT), MAX_POSITION_12BIT)


def position12(value: Any) -> int:
    """Validate a 12-bit position register value.

    Accepts a raw integer count (0-4095), an angle such as ``45deg`` or ``90°``,
    or a percentage of a full revolution such as ``25%``.
    """
    if isinstance(value, str) and value.endswith("%"):
        return _percent_to_position_12(value)
    if isinstance(value, str) and value.endswith(("°", "deg")):
        return _angle_to_position_12(value)
    return cv.int_range(min=0, max=MAX_POSITION_12BIT)(value)


def validate_hysteresis(value: Any) -> int:
    value = cv.float_(value)
    if value not in HYSTERESIS:
        raise cv.Invalid(
            f"Hysteresis must be one of {sorted(HYSTERESIS)} (LSBs), got {value}"
        )
    return HYSTERESIS[value]


CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(MT6701I2CComponent),
            cv.Optional(CONF_DIRECTION): cv.enum(DIRECTION, upper=True),
            cv.Optional(CONF_ZERO_OFFSET): position12,
            cv.Optional(CONF_HYSTERESIS): validate_hysteresis,
            cv.Optional(CONF_OUTPUT_MODE): cv.enum(OUTPUT_MODE, upper=True),
            cv.Optional(CONF_ABZ): cv.Schema(
                {
                    cv.Optional(CONF_PULSES_PER_REVOLUTION): cv.int_range(
                        min=1, max=1024
                    ),
                    cv.Optional(CONF_Z_PULSE_WIDTH): cv.enum(Z_PULSE_WIDTH, upper=True),
                }
            ),
            cv.Optional(CONF_UVW): cv.Schema(
                {
                    cv.Required(CONF_POLE_PAIRS): cv.int_range(min=1, max=16),
                }
            ),
            cv.Optional(CONF_OUT_PIN): cv.Schema(
                {
                    cv.Required(CONF_MODE): cv.enum(OUT_PIN_MODE, upper=True),
                    cv.Optional(CONF_PWM_FREQUENCY): cv.enum(PWM_FREQUENCY, upper=True),
                    cv.Optional(CONF_PWM_POLARITY): cv.enum(PWM_POLARITY, upper=True),
                    cv.Optional(CONF_ANALOG_START): position12,
                    cv.Optional(CONF_ANALOG_STOP): position12,
                }
            ),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(i2c.i2c_device_schema(0x06))
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await i2c.register_i2c_device(var, config)

    if (direction := config.get(CONF_DIRECTION)) is not None:
        cg.add(var.set_direction(direction))
    if (zero_offset := config.get(CONF_ZERO_OFFSET)) is not None:
        cg.add(var.set_zero_offset(zero_offset))
    if (hysteresis := config.get(CONF_HYSTERESIS)) is not None:
        cg.add(var.set_hysteresis(hysteresis))
    if (output_mode := config.get(CONF_OUTPUT_MODE)) is not None:
        cg.add(var.set_output_mode(output_mode))

    if abz := config.get(CONF_ABZ):
        if (ppr := abz.get(CONF_PULSES_PER_REVOLUTION)) is not None:
            cg.add(var.set_abz_pulses_per_revolution(ppr))
        if (width := abz.get(CONF_Z_PULSE_WIDTH)) is not None:
            cg.add(var.set_z_pulse_width(width))

    if uvw := config.get(CONF_UVW):
        cg.add(var.set_uvw_pole_pairs(uvw[CONF_POLE_PAIRS]))

    if out_pin := config.get(CONF_OUT_PIN):
        cg.add(var.set_out_pin_mode(out_pin[CONF_MODE]))
        if (freq := out_pin.get(CONF_PWM_FREQUENCY)) is not None:
            cg.add(var.set_pwm_frequency(freq))
        if (pol := out_pin.get(CONF_PWM_POLARITY)) is not None:
            cg.add(var.set_pwm_polarity(pol))
        if (start := out_pin.get(CONF_ANALOG_START)) is not None:
            cg.add(var.set_analog_start(start))
        if (stop := out_pin.get(CONF_ANALOG_STOP)) is not None:
            cg.add(var.set_analog_stop(stop))


MT6701_SAVE_EEPROM_ACTION_SCHEMA = automation.maybe_simple_id(
    {
        cv.Required(CONF_ID): cv.use_id(MT6701I2CComponent),
    }
)

automation.register_simple_action(
    "mt6701_i2c.save_eeprom",
    SaveEEPROMAction,
    MT6701_SAVE_EEPROM_ACTION_SCHEMA,
    synchronous=True,
)
