import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
    ICON_ARROW_EXPAND_VERTICAL,
    DEVICE_CLASS_DISTANCE,
)

DEPENDENCIES = ["uart"]

aj_sr04m_ns = cg.esphome_ns.namespace("aj_sr04m")
Ajsr04mComponent = aj_sr04m_ns.class_(
    "Ajsr04mComponent",
    sensor.Sensor,
    cg.PollingComponent,
    uart.UARTDevice,
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        Ajsr04mComponent,
        unit_of_measurement=UNIT_METER,
        icon=ICON_ARROW_EXPAND_VERTICAL,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
        device_class=DEVICE_CLASS_DISTANCE,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "aj_sr04m",
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
