import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import cover, uart_multi
from esphome.const import CONF_ADDRESS, CONF_ID

CODEOWNERS = ["@loongyh"]

AUTO_LOAD = ["uart_multi"]

dooya_ns = cg.esphome_ns.namespace("gm40")
GM40 = dooya_ns.class_("GM40", cover.Cover, cg.Component, uart_multi.UARTMultiDevice)

CONFIG_SCHEMA = cover.COVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GM40),
        cv.Optional(CONF_ADDRESS): cv.hex_uint8_t,
    }
).extend(uart_multi.UART_MULTI_DEVICE_SCHEMA)


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    yield uart_multi.register_uart_multi_device(var, config)
    yield cover.register_cover(var, config)

    if CONF_ADDRESS in config:
        address = config[CONF_ADDRESS]
        cg.add(var.set_address(address))
