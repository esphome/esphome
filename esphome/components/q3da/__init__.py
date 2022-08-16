import re

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart
from esphome.const import CONF_ID

CODEOWNERS = ["@vogt31337"]

DEPENDENCIES = ["uart"]

q3da_ns = cg.esphome_ns.namespace("q3da")
Q3DA = q3da_ns.class_("q3da", cg.Component, uart.UARTDevice)
MULTI_CONF = True

CONF_Q3DA_ID = "q3da_id"
CONF_OBIS_CODE = "obis_code"
CONF_SERVER_ID = "server_id"

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Q3DA),
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)


def obis_code(value):
    value = cv.string(value)
    match = re.match(r"^\d{1,3}-\d{1,3}:\d{1,3}\.\d{1,3}\.\d{1,3}$", value)
    if match is None:
        raise cv.Invalid(f"{value} is not a valid OBIS code")
    return value
