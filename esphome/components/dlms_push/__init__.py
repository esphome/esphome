import re

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_RECEIVE_TIMEOUT

DEPENDENCIES = ["uart"]

CONF_SHOW_LOG = "show_log"
CONF_CUSTOM_PATTERN = "custom_pattern"

# Define the namespace and the Hub component class
dlms_push_ns = cg.esphome_ns.namespace("dlms_push")
DlmsPushComponent = dlms_push_ns.class_(
    "DlmsPushComponent", cg.Component, uart.UARTDevice
)


def obis_code(value):
    value = cv.string(value)
    # Validate standard OBIS format: A.B.C.D.E.F (e.g., 1.0.1.8.0.255)
    match = re.match(r"^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$", value)
    if match is None:
        raise cv.Invalid(
            f"{value} is not a valid OBIS code (expected format: A.B.C.D.E.F)"
        )
    return value


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DlmsPushComponent),
            cv.Optional(
                CONF_RECEIVE_TIMEOUT, default="50ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_SHOW_LOG, default=False): cv.boolean,
            cv.Optional(CONF_CUSTOM_PATTERN, default=""): cv.string,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    # Apply configuration to the C++ component
    cg.add(var.set_receive_timeout(config[CONF_RECEIVE_TIMEOUT]))
    cg.add(var.set_show_log(config[CONF_SHOW_LOG]))
    cg.add(var.set_custom_pattern(config[CONF_CUSTOM_PATTERN]))
