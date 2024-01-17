import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, wk_base
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_CHANNEL,
    CONF_UART_ID,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
)

CODEOWNERS = ["@DrCoolZic"]
AUTO_LOAD = ["uart", "wk_base"]

MULTI_CONF = True
CONF_STOP_BITS = "stop_bits"
CONF_PARITY = "parity"
CONF_CRYSTAL = "crystal"
CONF_UART = "uart"
CONF_TEST_MODE = "test_mode"


def check_channel_wk2168(value):
    """Check duplicate channels and 4 channels maximum"""
    channel_uniq = []
    channel_dup = []
    for x in value[CONF_UART]:
        if x[CONF_CHANNEL] > 3:
            raise cv.Invalid(f"Invalid channel number: {x[CONF_CHANNEL]}")
        if x[CONF_CHANNEL] not in channel_uniq:
            channel_uniq.append(x[CONF_CHANNEL])
        else:
            channel_dup.append(x[CONF_CHANNEL])
    if len(channel_dup) > 0:
        raise cv.Invalid(f"Duplicate channel list: {channel_dup}")
    return value


def validate_pin_mode(value):
    """checks input/output mode inconsistency"""
    if not (value[CONF_MODE][CONF_INPUT] or value[CONF_MODE][CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_MODE][CONF_INPUT] and value[CONF_MODE][CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value


WK2168_PIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=8),
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_INPUT, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT, default=False): cv.boolean,
            },
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)
