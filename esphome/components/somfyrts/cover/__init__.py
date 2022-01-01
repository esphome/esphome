import esphome.codegen as cg
from esphome import pins
import esphome.config_validation as cv
from esphome.components import cover, uart
from esphome.const import (
    CONF_CLOSE_DURATION,
    CONF_FLOW_CONTROL_PIN,
    CONF_ID,
    CONF_INVERTED,
    CONF_OPEN_DURATION,
    CONF_CHANNEL,
)

CONF_NODE_ID_1 = "node_id_1"
CONF_NODE_ID_2 = "node_id_2"
CONF_NODE_ID_3 = "node_id_3"
DEPENDENCIES = ["uart"]
CODEOWNERS = ["@icarome"]

somfyrts_ns = cg.esphome_ns.namespace("somfyrts")
SomfyRTSCover = somfyrts_ns.class_(
    "SomfyRTSCover", cover.Cover, cg.Component, uart.UARTDevice
)

CONFIG_SCHEMA = (
    cover.COVER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(SomfyRTSCover),
            cv.Required(CONF_CHANNEL): cv.int_range(min=0, max=15),
            cv.Required(CONF_NODE_ID_1): cv.int_range(min=0, max=0xFF),
            cv.Required(CONF_NODE_ID_2): cv.int_range(min=0, max=0xFF),
            cv.Required(CONF_NODE_ID_3): cv.int_range(min=0, max=0xFF),
            cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
            cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_INVERTED, default=False): cv.boolean,
            cv.Optional(CONF_FLOW_CONTROL_PIN): pins.gpio_output_pin_schema,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await cover.register_cover(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    cg.add(var.set_channel(config[CONF_CHANNEL]))
    cg.add(
        var.set_node_id(
            config[CONF_NODE_ID_1], config[CONF_NODE_ID_2], config[CONF_NODE_ID_3]
        )
    )
    if CONF_INVERTED in config:
        cg.add(var.inverted(config[CONF_INVERTED]))
    if CONF_FLOW_CONTROL_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])
        cg.add(var.set_ctrl_pin(pin))
