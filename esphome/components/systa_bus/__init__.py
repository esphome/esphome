import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@Mat931"]

DEPENDENCIES = ["uart"]

MULTI_CONF = True

systa_bus_ns = cg.esphome_ns.namespace("systa_bus")
SystaBus = systa_bus_ns.class_("SystaBus", cg.Component, uart.UARTDevice)

CONF_SYSTA_BUS_ID = "systa_bus"

CONF_SYSTASOLAR_AQUA = "systasolar_aqua"

CONFIG_SCHEMA = uart.UART_DEVICE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(SystaBus),
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
