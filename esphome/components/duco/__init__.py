from __future__ import annotations

import esphome.codegen as cg
from esphome.components import uart
import esphome.config_validation as cv
from esphome.const import CONF_DISABLE_CRC, CONF_DISCOVERY, CONF_ID

CODEOWNERS = ["@kokx"]
DEPENDENCIES = ["uart"]

duco_ns = cg.esphome_ns.namespace("duco")
Duco = duco_ns.class_("Duco", cg.Component, uart.UARTDevice)
DucoDiscovery = duco_ns.class_(
    "DucoDiscovery", cg.PollingComponent, cg.Parented.template(Duco)
)
MULTI_CONF = True

CONF_DUCO_ID = "duco_id"
CONF_SEND_WAIT_TIME = "send_wait_time"

# A schema for components like sensors
DUCO_COMPONENT_SCHEMA = cv.Schema({cv.GenerateID(CONF_DUCO_ID): cv.use_id(Duco)})

DISCOVERY_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DucoDiscovery),
        }
    )
    .extend(cv.polling_component_schema("60s"))
    .extend(DUCO_COMPONENT_SCHEMA)
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(Duco),
            cv.Optional(
                CONF_SEND_WAIT_TIME, default="250ms"
            ): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_DISABLE_CRC, default=False): cv.boolean,
            cv.Optional(CONF_DISCOVERY): DISCOVERY_SCHEMA,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    cg.add_global(duco_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    await uart.register_uart_device(var, config)

    cg.add(var.set_send_wait_time(config[CONF_SEND_WAIT_TIME]))
    cg.add(var.set_disable_crc(config[CONF_DISABLE_CRC]))

    if CONF_DISCOVERY in config:
        discovery_config = config[CONF_DISCOVERY]
        discovery_var = cg.new_Pvariable(discovery_config[CONF_ID])
        await cg.register_component(discovery_var, discovery_config)

        cg.add(discovery_var.set_parent(var))
