import esphome.codegen as cg
from esphome.components import climate, uart
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import DOMAIN

CODEOWNERS = ["@Fexiven"]
DEPENDENCIES = ["uart"]

midea_portasplit_ns = cg.esphome_ns.namespace("midea_portasplit")
PortaSplitClimate = midea_portasplit_ns.class_(
    "PortaSplitClimate", climate.Climate, uart.UARTDevice, cg.Component
)

CONFIG_SCHEMA = (
    climate.climate_schema(PortaSplitClimate)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN, baud_rate=9600, require_tx=True, require_rx=True
)


async def to_code(config: ConfigType) -> None:
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
