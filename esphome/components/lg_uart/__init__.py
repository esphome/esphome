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
from esphome.const import CONF_ID

CODEOWNERS = ["@kquinsland"]
DEPENDENCIES = ["uart"]
MULTI_CONF = True

# Hub needs the ID of the UART and set ID
CONF_UART_ID = "uart_id"
CONF_SCREEN_NUM = "screen"


lg_uart = cg.esphome_ns.namespace("lg_uart")

LGUartHub = lg_uart.class_("LGUartHub", cg.PollingComponent, uart.UARTDevice)

CONFIG_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(LGUartHub),
        # TODO: confirm this, pretty sure that's the max
        cv.Optional(CONF_SCREEN_NUM, default=0): cv.int_range(min=0, max=99),
    }
)


# async def register_bedjet_child(var, config):
#     parent = await cg.get_variable(config[CONF_UART_ID])
#     cg.add(parent.register_child(var))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    # Screen ID / number
    cg.add(var.set_screen_number(config[CONF_SCREEN_NUM]))
