from esphome.components import uart
from esphome.components.gnss import GNSSComponent, gnss_schema, new_gnss
from esphome.types import ConfigType

from . import nmea_ns

DEPENDENCIES = ["uart"]

NMEAGNSS = nmea_ns.class_("NMEAGNSS", GNSSComponent, uart.UARTDevice)

CONFIG_SCHEMA = gnss_schema(NMEAGNSS).extend(uart.UART_DEVICE_SCHEMA)
FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema("nmea", require_rx=True)


async def to_code(config: ConfigType) -> None:
    var = await new_gnss(config)
    await uart.register_uart_device(var, config)
