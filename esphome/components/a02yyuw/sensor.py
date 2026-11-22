import esphome.codegen as cg
from esphome.components import sensor, uart
from esphome.const import ICON_ARROW_EXPAND_VERTICAL
from esphome.types import ConfigType

CODEOWNERS = ["@TH-Braemer"]
DEPENDENCIES = ["uart"]

a02yyuw_ns = cg.esphome_ns.namespace("a02yyuw")
A02yyuwComponent = a02yyuw_ns.class_(
    "A02yyuwComponent", sensor.Sensor, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = sensor.sensor_schema(
    A02yyuwComponent, kind=sensor.DISTANCE_MM, icon=ICON_ARROW_EXPAND_VERTICAL
).extend(uart.UART_DEVICE_SCHEMA)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "a02yyuw",
    baud_rate=9600,
    require_tx=False,
    require_rx=True,
    data_bits=8,
    parity=None,
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
