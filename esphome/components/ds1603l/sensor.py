import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_DISTANCE,
    STATE_CLASS_MEASUREMENT,
    UNIT_MILLIMETER,
)
from esphome.types import ConfigType

CODEOWNERS = ["@JakeLC15"]
DEPENDENCIES = ["uart"]

ds1603l_ns = cg.esphome_ns.namespace("ds1603l")
DS1603L = ds1603l_ns.class_("DS1603L", sensor.Sensor, cg.Component, uart.UARTDevice)


CONFIG_SCHEMA = (
    sensor.sensor_schema(
        DS1603L,
        unit_of_measurement=UNIT_MILLIMETER,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_DISTANCE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ds1603l",
    baud_rate=9600,
    require_tx=False,
    require_rx=True,
    data_bits=8,
    stop_bits=1,
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
