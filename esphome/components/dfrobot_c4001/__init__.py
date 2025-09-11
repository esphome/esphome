import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

DEPENDENCIES = ["uart"]
MULTI_CONF = True
CODEOWNERS = ["zhixin.liu@dfrobot.com"]


dfrobot_c4001_ns = cg.esphome_ns.namespace("dfrobot_c4001")
c4001Component = dfrobot_c4001_ns.class_("c4001Component", cg.Component, uart.UARTDevice)


CONF_C4001_ID = "c4001_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(c4001Component),
    }
).extend(uart.UART_DEVICE_SCHEMA).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
