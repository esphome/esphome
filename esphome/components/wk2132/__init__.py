import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import (
    CONF_BAUD_RATE,
    CONF_CHANNEL,
    # CONF_ID,
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

wk2132_ns = cg.esphome_ns.namespace("wk2132")
WK2132Component = wk2132_ns.class_("WK2132Component", cg.Component)
WK2132Channel = wk2132_ns.class_("WK2132Channel", uart.UARTComponent)


# def check_conf(value):
#     if (
#         len(value[CONF_UART]) > 1
#         and value[CONF_UART][0][CONF_CHANNEL] == value[CONF_UART][1][CONF_CHANNEL]
#     ):
#         raise cv.Invalid("Duplicate channel number")
#     return value


# WK2132_SCHEMA = cv.All(
#     cv.Schema(
#         {
#             cv.GenerateID(): cv.declare_id(WK2132Component),
#             cv.Optional(CONF_CRYSTAL, default=14745600): cv.int_,
#             cv.Optional(CONF_TEST_MODE, default=0): cv.int_,
#             cv.Required(CONF_UART): cv.ensure_list(
#                 {
#                     cv.Required(CONF_UART_ID): cv.declare_id(WK2132Channel),
#                     cv.Optional(CONF_CHANNEL, default=0): cv.int_range(min=0, max=1),
#                     cv.Required(CONF_BAUD_RATE): cv.int_range(min=1),
#                     cv.Optional(CONF_STOP_BITS, default=1): cv.one_of(1, 2, int=True),
#                     cv.Optional(CONF_PARITY, default="NONE"): cv.enum(
#                         uart.UART_PARITY_OPTIONS, upper=True
#                     ),
#                 }
#             ),
#         }
#     ).extend(cv.COMPONENT_SCHEMA),
#     check_conf,
# )


WK2132_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(WK2132Component),
        cv.Optional(CONF_CRYSTAL, default=14745600): cv.int_,
        cv.Optional(CONF_TEST_MODE, default=0): cv.int_,
        cv.Required(CONF_UART): cv.ensure_list(
            {
                cv.Required(CONF_UART_ID): cv.declare_id(WK2132Channel),
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


async def register_wk2132(var, config):
    """Register an wk2132 device with the given config."""
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
