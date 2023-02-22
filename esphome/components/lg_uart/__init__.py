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
from esphome.const import CONF_UART_ID, CONF_ID

CODEOWNERS = ["@kquinsland"]
DEPENDENCIES = ["uart"]
MULTI_CONF = False

# Hub needs the ID of the UART and set ID
CONF_SCREEN_NUMBER = "screen_number"

# Clients
CONF_LG_UART_ID = "lg_uart_id"
CONF_LG_UART_CMD = "lg_uart_cmd"


lg_uart_ns = cg.esphome_ns.namespace("lg_uart")
LGUartHub = lg_uart_ns.class_("LGUartHub", cg.PollingComponent, uart.UARTDevice)

# the root component
CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(LGUartHub),
        # We need to know which uart device to use
        cv.Required(CONF_UART_ID): cv.use_id(uart.CONF_ID),
        cv.Optional(CONF_SCREEN_NUMBER, default=0): cv.int_range(min=0, max=99),
    }
)

# Every component that uses us as a platform will need this
LG_UART_CLIENT_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_LG_UART_ID): cv.use_id(LGUartHub),
    }
)


async def register_lg_uart_child(var, config):
    parent = await cg.get_variable(config[CONF_LG_UART_ID])
    cmd_char = config[CONF_LG_UART_CMD][1]
    cg.add(parent.register_child(var, cmd_char))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # ID of the UART component
    await uart.register_uart_device(var, config)

    # Screen ID / number
    cg.add(var.set_screen_number(config[CONF_SCREEN_NUMBER]))
    return var


def validate_uart_cmd_len(config):
    """
    Test that the uart command char from user is exactly 2 chars long
    """
    if len(config[CONF_LG_UART_CMD]) != 2:
        raise cv.Invalid(f"{CONF_LG_UART_CMD}: must be two characters in length.")
    return config
