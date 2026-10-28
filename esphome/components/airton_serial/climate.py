import esphome.codegen as cg
from esphome.components import climate, uart
import esphome.config_validation as cv

CODEOWNERS = ["@guiguid"]
DEPENDENCIES = ["climate", "uart"]

airton_serial_ns = cg.esphome_ns.namespace("airton_serial")
AirtonSerialClimate = airton_serial_ns.class_(
    "AirtonSerialClimate", uart.UARTDevice, climate.Climate, cg.Component
)

CONFIG_SCHEMA = cv.All(
    climate.climate_schema(AirtonSerialClimate)
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "airton_serial", baud_rate=9600, require_rx=True, require_tx=True
)


async def to_code(config):
    var = await climate.new_climate(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
