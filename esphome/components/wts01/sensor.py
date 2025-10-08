import esphome.codegen as cg
from esphome.components import sensor, uart
import esphome.config_validation as cv
from esphome.const import (
    DEVICE_CLASS_TEMPERATURE,
    STATE_CLASS_MEASUREMENT,
    UNIT_CELSIUS,
)

CONF_WTS01_ID = "wts01_id"
CODEOWNERS = ["@alepee"]
DEPENDENCIES = ["uart"]

wts01_ns = cg.esphome_ns.namespace("wts01")
WTS01Sensor = wts01_ns.class_(
    "WTS01Sensor", cg.Component, uart.UARTDevice, sensor.Sensor
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        WTS01Sensor,
        unit_of_measurement=UNIT_CELSIUS,
        accuracy_decimals=1,
        device_class=DEVICE_CLASS_TEMPERATURE,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "wts01",
    baud_rate=9600,
    require_rx=True,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
