import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_CHANNEL,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
)

CODEOWNERS = ["@DrCoolZic"]
AUTO_LOAD = ["uart"]

MULTI_CONF = True
CONF_STOP_BITS = "stop_bits"
CONF_PARITY = "parity"
CONF_CRYSTAL = "crystal"
CONF_UART = "uart"
CONF_TEST_MODE = "test_mode"

weikai_ns = cg.esphome_ns.namespace("weikai")
WeikaiComponent = weikai_ns.class_("WeikaiComponent", cg.Component)
WeikaiChannel = weikai_ns.class_("WeikaiChannel", uart.UARTComponent)


def check_channel_max(value, max):
    channel_uniq = []
    channel_dup = []
    for x in value[CONF_UART]:
        if x[CONF_CHANNEL] > max - 1:
            raise cv.Invalid(f"Invalid channel number: {x[CONF_CHANNEL]}")
        if x[CONF_CHANNEL] not in channel_uniq:
            channel_uniq.append(x[CONF_CHANNEL])
        else:
            channel_dup.append(x[CONF_CHANNEL])
    if len(channel_dup) > 0:
        raise cv.Invalid(f"Duplicate channel list: {channel_dup}")
    return value


def check_channel_max_4(value):
    return check_channel_max(value, 4)


def check_channel_max_2(value):
    return check_channel_max(value, 2)


WKBASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WeikaiComponent),
        cv.Optional(CONF_CRYSTAL, default=14745600): cv.int_,
        cv.Optional(CONF_TEST_MODE, default=0): cv.int_,
        cv.Required(CONF_UART): cv.ensure_list(
            {
                cv.Required(CONF_ID): cv.declare_id(WeikaiChannel),
                cv.Optional(CONF_CHANNEL, default=0): cv.int_range(min=0, max=3),
                cv.Required(CONF_BAUD_RATE): cv.int_range(min=1),
                cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
                cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
                    uart.UART_PARITY_OPTIONS, upper=True
                ),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def register_weikai(var, config):
    """Register an weikai device with the given config."""
    cg.add(var.set_crystal(config[CONF_CRYSTAL]))
    cg.add(var.set_test_mode(config[CONF_TEST_MODE]))
    await cg.register_component(var, config)
    for uart_elem in config[CONF_UART]:
        chan = cg.new_Pvariable(uart_elem[CONF_ID])
        cg.add(chan.set_channel_name(str(uart_elem[CONF_ID])))
        cg.add(chan.set_parent(var))
        cg.add(chan.set_channel(uart_elem[CONF_CHANNEL]))
        cg.add(chan.set_baud_rate(uart_elem[CONF_BAUD_RATE]))
        cg.add(chan.set_stop_bits(uart_elem[CONF_STOP_BITS]))
        cg.add(chan.set_parity(uart_elem[CONF_PARITY]))


def validate_pin_mode(value):
    """Checks input/output mode inconsistency"""
    if not (value[CONF_MODE][CONF_INPUT] or value[CONF_MODE][CONF_OUTPUT]):
        raise cv.Invalid("Mode must be either input or output")
    if value[CONF_MODE][CONF_INPUT] and value[CONF_MODE][CONF_OUTPUT]:
        raise cv.Invalid("Mode must be either input or output")
    return value


WEIKAI_PIN_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NUMBER): cv.int_range(min=0, max=7),
        cv.Optional(CONF_MODE, default={}): cv.All(
            {
                cv.Optional(CONF_INPUT, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT, default=False): cv.boolean,
            },
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    }
)
