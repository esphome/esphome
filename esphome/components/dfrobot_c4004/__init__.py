import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
MULTI_CONF = True
CODEOWNERS = ["@jiaziui"]

dfrobot_c4004_ns = cg.esphome_ns.namespace("dfrobot_c4004")
C4004Component = dfrobot_c4004_ns.class_(
    "C4004Component", cg.Component, uart.UARTDevice
)

CONF_C4004_ID = "c4004_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(C4004Component),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
