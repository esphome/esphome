import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, sensor
from esphome.const import (
    ICON_CHEMICAL_WEAPON,
    UNIT_PARTS_PER_MILLION,
    STATE_CLASS_MEASUREMENT,
)

CODEOWNERS = ["@cwitting"]
DEPENDENCIES = ["uart"]

sen0231_sensor_ns = cg.esphome_ns.namespace("sen0231_sensor")
Sen0231Sensor = sen0231_sensor_ns.class_(
    "Sen0231Sensor", cg.PollingComponent, uart.UARTDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        Sen0231Sensor,
        unit_of_measurement=UNIT_PARTS_PER_MILLION,
        icon=ICON_CHEMICAL_WEAPON,
        accuracy_decimals=3,
        state_class=STATE_CLASS_MEASUREMENT,
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = await sensor.new_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
