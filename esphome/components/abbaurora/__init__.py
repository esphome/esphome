import esphome.codegen as cg
import esphome.config_validation as cv

from esphome.components import sensor,uart
from esphome.cpp_helpers import gpio_pin_expression
from esphome.const import (
    CONF_FLOW_CONTROL_PIN,
    CONF_ID,
    CONF_ADDRESS,
)
from esphome import pins

CODEOWNERS = ["@ajvdw"]

DEPENDENCIES = ["uart"]

AUTO_LOAD = ["sensor", "text_sensor"]

CONF_ABBAURORA_ID = "abbaurora_id"

abbaurora_ns = cg.esphome_ns.namespace("esphome::abbaurora")
ABBAurora = abbaurora_ns.class_("ABBAuroraComponent", uart.UARTDevice, cg.Component )

ABBAURORA_COMPONENT_SCHEMA = cv.COMPONENT_SCHEMA.extend(
    {
        cv.Required(CONF_ABBAURORA_ID): cv.use_id(ABBAurora),
    }
)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ABBAurora),
            cv.Required(CONF_FLOW_CONTROL_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_ADDRESS, default=2): cv.int_range(min=0, max=1023),
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
    .extend(uart.UART_DEVICE_SCHEMA)
)

async def to_code(config):
    cg.add_global(abbaurora_ns.using)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)
    if CONF_FLOW_CONTROL_PIN in config:
        pin = await gpio_pin_expression(config[CONF_FLOW_CONTROL_PIN])
        cg.add(var.set_flow_control_pin(pin))
    if CONF_ADDRESS in config:
        cg.add(var.set_address(config[CONF_ADDRESS]))

