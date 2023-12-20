import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

CODEOWNERS = ["@Mat931"]
MULTI_CONF = True
DEPENDENCIES = ["uart"]

CONF_SUN_GTIL2_ID = "sun_gtil2_id"

sun_gtil2_ns = cg.esphome_ns.namespace("sun_gtil2")

SunGTIL2Component = sun_gtil2_ns.class_("SunGTIL2", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SunGTIL2Component),
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
