import esphome.codegen as cg
from esphome.components import sensor, uart
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    ICON_ARROW_EXPAND_VERTICAL,
    DEVICE_CLASS_DISTANCE,
)

CODEOWNERS = ["@TH-Braemer"]
DEPENDENCIES = ["uart"]
UNIT_MILLIMETERS = "mm"

a02yyuw_ns = cg.esphome_ns.namespace("a02yyuw")
A02yyuwComponent = a02yyuw_ns.class_(
    "A02yyuwComponent", sensor.Sensor, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = sensor.sensor_schema(
    A02yyuwComponent,
    unit_of_measurement=UNIT_MILLIMETERS,
    icon=ICON_ARROW_EXPAND_VERTICAL,
    accuracy_decimals=0,
    state_class=STATE_CLASS_MEASUREMENT,
    device_class=DEVICE_CLASS_DISTANCE,
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


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
