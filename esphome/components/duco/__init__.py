from __future__ import annotations

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_DISABLE_CRC, CONF_ID

DEPENDENCIES = ["uart"]

duco_ns = cg.esphome_ns.namespace("duco")
Duco = duco_ns.class_("Duco", cg.Component, uart.UARTDevice)
# ModbusDevice = duco_ns.class_("ModbusDevice")
MULTI_CONF = True

CONF_DUCO_ID = "duco_id"
CONF_SEND_WAIT_TIME = "send_wait_time"

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Duco),
            cv.Optional(
                CONF_SEND_WAIT_TIME, default="250ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DISABLE_CRC, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

# A schema for components like sensors
DUCO_COMPONENT_SCHEMA = cv.Schema({cv.GenerateID(CONF_DUCO_ID): cv.use_id(Duco)})


async def to_code(config):
    cg.add_global(duco_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await uart.register_uart_device(var, config)

    cg.add(var.set_send_wait_time(config[CONF_SEND_WAIT_TIME]))
    cg.add(var.set_disable_crc(config[CONF_DISABLE_CRC]))
