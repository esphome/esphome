import esphome.codegen as cg
from esphome.components import button, uart
import esphome.config_validation as cv
from esphome.const import CONF_DATA

from .. import payload_table, uart_ns, validate_raw_payload

CODEOWNERS = ["@ssieb"]

DEPENDENCIES = ["uart"]

UARTButton = uart_ns.class_("UARTButton", button.Button, uart.UARTDevice, cg.Component)


CONFIG_SCHEMA = (
    button.button_schema(UARTButton)
    .extend(
        {
            cv.Required(CONF_DATA): validate_raw_payload,
        }
    )
    .extend(uart.UART_DEVICE_SCHEMA)
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await button.new_button(config)
    await cg.register_component(var, config)
    await uart.register_uart_device(var, config)

    data = config[CONF_DATA]
    cg.add(var.set_data(payload_table(data), len(data)))
