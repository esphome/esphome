import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.types import ConfigType

CODEOWNERS = ["@Mat931"]
DOMAIN = "sun_gtil2"
MULTI_CONF = True
DEPENDENCIES = ["uart"]

CONF_SUN_GTIL2_ID = "sun_gtil2_id"

sun_gtil2_ns = cg.esphome_ns.namespace("sun_gtil2")

SunGTIL2Component = sun_gtil2_ns.class_("SunGTIL2", cg.Component, uart.UARTDevice)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SunGTIL2Component),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: ConfigType) -> None:
    await uart.new_uart_component(config)
