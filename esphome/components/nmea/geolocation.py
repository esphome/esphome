from esphome.components import uart
from esphome.components.geolocation import (
    GeolocationComponent,
    geolocation_schema,
    new_geolocation,
)
from esphome.types import ConfigType

from . import nmea_ns

DEPENDENCIES = ["uart"]

NMEAGNSS = nmea_ns.class_("NMEAGNSS", GeolocationComponent, uart.UARTDevice)

CONFIG_SCHEMA = geolocation_schema(NMEAGNSS).extend(uart.UART_DEVICE_SCHEMA)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("nmea", require_rx=True)


async def to_code(config: ConfigType) -> None:
    var = await new_geolocation(config)
    await uart.register_uart_device(var, config)
