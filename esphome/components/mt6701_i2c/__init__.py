from typing import Any

from esphome import automation
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.const import CONF_PWM_FREQUENCY
import esphome.config_validation as cv
from esphome.const import CONF_DIRECTION, CONF_HYSTERESIS, CONF_ID, CONF_MODE
from esphome.types import ConfigType

from ..mt6701 import MT6701Component

CODEOWNERS = ["@slimcdk"]
AUTO_LOAD = ["mt6701"]
DEPENDENCIES = ["i2c"]
DOMAIN = "mt6701_i2c"
MULTI_CONF = True

mt6701_i2c_ns = cg.esphome_ns.namespace("mt6701_i2c")
MT6701I2CComponent = mt6701_i2c_ns.class_(
    "MT6701I2CComponent", MT6701Component, i2c.I2CDevice
)

CONF_ZERO_OFFSET = "zero_offset"
CONF_OUTPUT_MODE = "output_mode"
CONF_ABZ = "abz"
CONF_PULSES_PER_REVOLUTION = "pulses_per_revolution"
CONF_Z_PULSE_WIDTH = "z_pulse_width"
CONF_UVW = "uvw"
CONF_POLE_PAIRS = "pole_pairs"
CONF_OUT_PIN = "out_pin"
CONF_PWM_POLARITY = "pwm_polarity"
CONF_ANALOG_START = "analog_start"
CONF_ANALOG_STOP = "analog_stop"

# DIR bit: 1 counts clockwise
DIRECTION = {
    "CLOCKWISE": 1,
    "COUNTERCLOCKWISE": 0,
}

# Output LSBs to the HYST register code
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

# Zero offset and analog start/stop are 12-bit positions over one revolution
RESOLUTION_12BIT = 4096
MAX_POSITION_12BIT = RESOLUTION_12BIT - 1
ANGLE_TO_POSITION_12 = RESOLUTION_12BIT / 360


def _angle_to_position_12(value: Any) -> int:
    value = cv.float_range(min=0, max=360)(
        cv.float_with_unit("angle", "(°|deg)")(value)
    )
    # 360° is not representable; clamp instead of wrapping to 0
    return min(round(value * ANGLE_TO_POSITION_12), MAX_POSITION_12BIT)


def _percent_to_position_12(value: Any) -> int:
    value = cv.percentage(value)
    return min(round(value * RESOLUTION_12BIT), MAX_POSITION_12BIT)


def position12(value: Any) -> int:
    """A 12-bit position: a raw count, an angle (``45deg``, ``90°``) or a share of a turn (``25%``)."""
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
    .extend(cv.COMPONENT_SCHEMA)
    .extend(i2c.i2c_device_schema(0x06))
)


async def to_code(config: ConfigType) -> None:
    var = await i2c.new_i2c_component(config)

    cg.add_optional(var.set_direction, config.get(CONF_DIRECTION))
    cg.add_optional(var.set_zero_offset, config.get(CONF_ZERO_OFFSET))
    cg.add_optional(var.set_hysteresis, config.get(CONF_HYSTERESIS))
    cg.add_optional(var.set_output_mode, config.get(CONF_OUTPUT_MODE))

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

automation.register_apply_action(
    "mt6701_i2c.save_eeprom",
    MT6701_SAVE_EEPROM_ACTION_SCHEMA,
    automation.ApplyCall("save_eeprom()"),
)
