import math
import struct

import esphome.codegen as cg
from esphome.components import output
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_INVERTED,
    CONF_MAX_POWER,
    CONF_MIN_POWER,
    CONF_PIN,
)
from esphome.types import ConfigType

from .. import CONF_SX1509_ID, SX1509Component

DEPENDENCIES = ["sx1509"]

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(output.ApplyFloatOutput),
        cv.GenerateID(CONF_SX1509_ID): cv.use_id(SX1509Component),
        cv.Required(CONF_PIN): cv.int_range(min=0, max=15),
    }
)


FINAL_VALIDATE_SCHEMA = output.final_validate_unique_channel(
    platform="sx1509", hub_key=CONF_SX1509_ID, channel_key=CONF_PIN
)

_request_led_slot = cg.slot_counter("SX1509_LED_CHANNEL_COUNT")


def _f32(value: float) -> float:
    return struct.unpack("f", struct.pack("f", value))[0]


def initial_duty(config: ConfigType) -> int:
    """The duty FloatOutput::turn_off() writes, in float math like the C++ code."""
    # set_max_power() runs first and clamps against the default min_power of 0
    max_power = min(max(_f32(config.get(CONF_MAX_POWER, 1.0)), 0.0), 1.0)
    level = 0.0
    if not config[output.CONF_ZERO_MEANS_ZERO]:
        level = min(max(_f32(config.get(CONF_MIN_POWER, 0.0)), 0.0), max_power)
    if config.get(CONF_INVERTED):
        level = _f32(1.0 - level)
    return int(math.floor(_f32(level * 255) + 0.5))


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_SX1509_ID])
    await output.sub_float_outputs(config, parent=hub).create(
        config, None, config[CONF_PIN], apply="write_led(index, state)"
    )
    cg.add(hub.register_led_channel(config[CONF_PIN], initial_duty(config)))
    _request_led_slot(str(config[CONF_SX1509_ID]))
