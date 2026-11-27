import esphome.codegen as cg
from esphome.components import climate, uart
import esphome.config_validation as cv
from esphome.types import ConfigType

from . import DOMAIN, AirtonSerialClimate

DEPENDENCIES = ["uart"]

CONFIG_SCHEMA = (
    climate.climate_schema(AirtonSerialClimate)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    DOMAIN,
    baud_rate=9600,
    require_rx=True,
    require_tx=True,
    data_bits=8,
    parity="NONE",
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
