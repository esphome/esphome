import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.components import binary_sensor, uart
from esphome.const import (
    DEVICE_CLASS_MOTION,
    ICON_MOTION_SENSOR
)

CODEOWNERS = ["lorki97", "florianL21"]

DEPENDENCIES = ["uart"]
AUTO_LOAD = ["binary_sensor"]

mr24d11c10_ns = cg.esphome_ns.namespace("mr24d11c10")

CONF_REQ_KEY = 'req_key'

MR24D11C10Sensor = mr24d11c10_ns.class_(
    "MR24D11C10Sensor", binary_sensor.BinarySensor, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = (
    binary_sensor.binary_sensor_schema(
        MR24D11C10Sensor,
        device_class=DEVICE_CLASS_MOTION,
        icon=ICON_MOTION_SENSOR,
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend({
        cv.Required(CONF_REQ_KEY): cv.boolean,
    })
)


async def to_code(config):
    var = await binary_sensor.new_binary_sensor(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_req_key(config[CONF_REQ_KEY]))
