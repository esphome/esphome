"""
Simple UART based component to set/poll various settings for LG screens.
LG provides generic documentation that describes many different things that can be set.
Not all models/firmware-versions support setting all things.

Additionally, not all models support polling the state of various things.
Each of the different things that can be controlled can be best represented with a switch (power/mute) or
    sensor (volume level?) or slider (volume) ... etc
"""

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID, CONF_COMMAND

CODEOWNERS = ["@kquinsland"]
DEPENDENCIES = ["uart"]

# Hub needs the ID of the UART and set ID
CONF_SCREEN_NUMBER = "screen_number"

# Clients
CONF_LG_UART_ID = "lg_uart_id"

CONF_DECODE_BASE = "decode_base"
DECODE_BASE_TYPES = [10, 16]


lg_uart_ns = cg.esphome_ns.namespace("lg_uart")
LGUartHub = lg_uart_ns.class_("LGUartHub", cg.PollingComponent, uart.UARTDevice)

# the root component
CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(LGUartHub),
        # Docs indicate that ID of 0 is broadcast / all screens.
        # By default, each comes from the factory set as 1.
        cv.Optional(CONF_SCREEN_NUMBER, default=1): cv.int_range(min=0, max=99),
    }
    # We need to know which uart device to use
).extend(uart.UART_DEVICE_SCHEMA)


# Every component that uses us as a platform will need this
LG_UART_CLIENT_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LG_UART_ID): cv.use_id(LGUartHub),
        cv.Required(CONF_COMMAND): cv.All(cv.string_strict, cv.Length(min=2, max=2)),
    },
)


async def register_lg_uart_child(var, config):
    parent = await cg.get_variable(config[CONF_LG_UART_ID])
    cmd_char = config[CONF_COMMAND][1]
    cg.add(parent.register_child(var, cmd_char))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # ID of the UART component
    await uart.register_uart_device(var, config)

    # Screen ID / number
    cg.add(var.set_screen_number(config[CONF_SCREEN_NUMBER]))
    return var
