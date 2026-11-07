import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_PM25,
    ICON_BLUR,
    STATE_CLASS_MEASUREMENT,
    UNIT_MICROGRAMS_PER_CUBIC_METER,
)
from esphome.types import ConfigType

CODEOWNERS = ["@ch604"]
DEPENDENCIES = ["uart"]

d01_ns = cg.esphome_ns.namespace("d01")
D01SensorComponent = d01_ns.class_(
    "D01SensorComponent", sensor.Sensor, uart.UARTDevice, cg.Component
)


CONFIG_SCHEMA = (
    sensor.sensor_schema(
        D01SensorComponent,
        unit_of_measurement=UNIT_MICROGRAMS_PER_CUBIC_METER,
        icon=ICON_BLUR,
        accuracy_decimals=0,
        device_class=DEVICE_CLASS_PM25,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "d01",
    baud_rate=9600,
    require_rx=True,
    require_tx=False,
)


async def to_code(config: ConfigType) -> None:
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
