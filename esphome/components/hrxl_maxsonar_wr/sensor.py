import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor, uart
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_EMPTY,
    STATE_CLASS_MEASUREMENT,
    UNIT_METER,
    ICON_ARROW_EXPAND_VERTICAL,
)

CODEOWNERS = ["@netmikey"]
DEPENDENCIES = ["uart"]

hrxlmaxsonarwr_ns = cg.esphome_ns.namespace("hrxlmaxsonarwr")
HrxlMaxsonarWrComponent = hrxlmaxsonarwr_ns.class_(
    "HrxlMaxsonarWrComponent", sensor.Sensor, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = (
    sensor.sensor_schema(
        UNIT_METER,
        ICON_ARROW_EXPAND_VERTICAL,
        3,
        DEVICE_CLASS_EMPTY,
        STATE_CLASS_MEASUREMENT,
    )
    .extend(
        {
            cv.GenerateID(): cv.declare_id(HrxlMaxsonarWrComponent),
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await sensor.register_sensor(var, config)
    await uart.register_uart_device(var, config)
