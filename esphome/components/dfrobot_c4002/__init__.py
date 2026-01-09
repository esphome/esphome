import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
MULTI_CONF = True
CODEOWNERS = ["@jiaziui"]

dfrobot_c4002_ns = cg.esphome_ns.namespace("dfrobot_c4002")
C4002Component = dfrobot_c4002_ns.class_(
    "C4002Component", cg.Component, uart.UARTDevice
)

CONF_C4002_ID = "c4002_id"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(C4002Component),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
