import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_CHANNEL,
    CONF_UART_ID,
)

CODEOWNERS = ["@DrCoolZic"]
AUTO_LOAD = ["uart"]

MULTI_CONF = True
CONF_STOP_BITS = "stop_bits"
CONF_PARITY = "parity"
CONF_CRYSTAL = "crystal"
CONF_UART = "uart"
CONF_TEST_MODE = "test_mode"

wk_base_ns = cg.esphome_ns.namespace("wk_base")
WKBaseComponent = wk_base_ns.class_("WKBaseComponent", cg.Component)
WKBaseChannel = wk_base_ns.class_("WKBaseChannel", uart.UARTComponent)


def check_duplicate(value):
    channel_uniq = []
    channel_dup = []
    for x in value[CONF_UART]:
        if x[CONF_CHANNEL] not in channel_uniq:
            channel_uniq.append(x[CONF_CHANNEL])
        else:
            channel_dup.append(x[CONF_CHANNEL])
    if len(channel_dup) > 0:
        raise cv.Invalid(f"Duplicate channel list: {channel_dup}")
    return value


WKBASE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WKBaseComponent),
        cv.Optional(CONF_CRYSTAL, default=14745600): cv.int_,
        cv.Optional(CONF_TEST_MODE, default=0): cv.int_,
        cv.Required(CONF_UART): cv.ensure_list(
            {
                cv.Required(CONF_UART_ID): cv.declare_id(WKBaseChannel),
                cv.Optional(CONF_CHANNEL, default=0): cv.int_range(min=0, max=1),
                cv.Required(CONF_BAUD_RATE): cv.int_range(min=1),
                cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
                cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
                    uart.UART_PARITY_OPTIONS, upper=True
                ),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def register_wk_base(var, config):
    """Register an wk_base device with the given config."""
    cg.add(var.set_crystal(config[CONF_CRYSTAL]))
    cg.add(var.set_test_mode(config[CONF_TEST_MODE]))
    await cg.register_component(var, config)
    for uart_elem in config[CONF_UART]:
        chan = cg.new_Pvariable(uart_elem[CONF_UART_ID])
        cg.add(chan.set_channel_name(str(uart_elem[CONF_UART_ID])))
        cg.add(chan.set_parent(var))
        cg.add(chan.set_channel(uart_elem[CONF_CHANNEL]))
        cg.add(chan.set_baud_rate(uart_elem[CONF_BAUD_RATE]))
        cg.add(chan.set_stop_bits(uart_elem[CONF_STOP_BITS]))
        cg.add(chan.set_parity(uart_elem[CONF_PARITY]))
