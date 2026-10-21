import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import CONF_CHANNEL, CONF_CURRENT, CONF_ID
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import LP5562Output

DEPENDENCIES = ["lp5562"]

CONF_LP5562_ID = "lp5562_id"
# Chip reset value of the current registers, 17.5 mA; matches currents_ in lp5562_output.h
CURRENT_RESET_TENTHS = 175

# Indices match the LP5562_REG_PWM lookup table in lp5562_output.cpp
LP5562_CHANNELS = {
    "blue": 0,
    "green": 1,
    "red": 2,
    "white": 3,
}


def _tenths(amps: float) -> int:
    """The register value of a current: steps of 0.1 mA."""
    return round(amps * 10000)


def _validate_current(amps: float) -> float:
    if abs(amps * 10000 - _tenths(amps)) > 1e-6:
        raise cv.Invalid("current must be a multiple of 0.1mA")
    if not 0 <= _tenths(amps) <= 255:
        raise cv.Invalid("current must be between 0mA and 25.5mA")
    return amps


CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.GenerateID(CONF_LP5562_ID): cv.use_id(LP5562Output),
        cv.Required(CONF_CHANNEL): cv.enum(LP5562_CHANNELS, lower=True),
        cv.Optional(CONF_CURRENT, default="17.5mA"): cv.All(
            cv.current, _validate_current
        ),
    }
)


def _final_validate(config: ConfigType) -> ConfigType:
    # Two outputs on one channel would fight over its PWM, and the last current would win
    used = fv.full_config.get().data.setdefault("lp5562_channels", set())
    hub, channel = str(config[CONF_LP5562_ID]), str(config[CONF_CHANNEL])
    if (hub, channel) in used:
        raise cv.Invalid(
            f"Channel '{channel}' of '{hub}' is already used by another output",
            [CONF_CHANNEL],
        )
    used.add((hub, channel))
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    channel = cg.safe_exp(config[CONF_CHANNEL])
    await output.sub_float_outputs(config, parent=config[CONF_LP5562_ID]).create(
        config, None, apply=f"write_channel({channel}, state)"
    )
    # The hub already holds the chip's reset value, 17.5 mA
    if (current := _tenths(config[CONF_CURRENT])) != CURRENT_RESET_TENTHS:
        parent = await cg.get_variable(config[CONF_LP5562_ID])
        cg.add(parent.set_channel_current(channel, current))
