import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import uart, light
from esphome.const import (
    CONF_OUTPUT_ID,
)

CONF_USE_RM433_REMOTE = "use_rm433_remote"

DEPENDENCIES = ["uart", "light"]

sonoff_d1_ns = cg.esphome_ns.namespace("sonoff_d1")
SonoffD1Output = sonoff_d1_ns.class_(
    "SonoffD1Output", cg.Component, uart.UARTDevice, light.LightOutput
)

CONFIG_SCHEMA = (
    light.BRIGHTNESS_ONLY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SonoffD1Output),
            cv.Optional(CONF_USE_RM433_REMOTE, default=False): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if CONF_USE_RM433_REMOTE in config:
        cg.add(var.set_use_rm433_remote(config[CONF_USE_RM433_REMOTE]))

    await light.register_light(var, config)
