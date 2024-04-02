import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_CENTIMETER,
    ICON_ARROW_EXPAND_VERTICAL,
)

DEPENDENCIES = ["uart"]

ultrasonic_uart_ns = cg.esphome_ns.namespace("ultrasonic_uart")
UltrasonicSensorComponent_UART = ultrasonic_uart_ns.class_(
    "UltrasonicSensorComponent_UART",
    sensor.Sensor,
    cg.PollingComponent,
    uart.UARTDevice,
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        UltrasonicSensorComponent_UART,
        unit_of_measurement=UNIT_CENTIMETER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=2,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "ultrasonic_uart",
    baud_rate=9600,
    require_tx=True,
    require_rx=True,
    data_bits=8,
    parity=None,
    stop_bits=1,
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
