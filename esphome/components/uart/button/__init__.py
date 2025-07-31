import esphome.codegen as cg
from esphome.components import button, uart
import esphome.config_validation as cv
from esphome.const import CONF_DATA
from esphome.core import HexInt

from .. import uart_ns, validate_raw_data

CODEOWNERS = ["@ssieb"]

DEPENDENCIES = ["uart"]

UARTButton = uart_ns.class_("UARTButton", button.Button, uart.UARTDevice, cg.Component)


CONFIG_SCHEMA = (
    button.button_schema(UARTButton)
    .extend(
        {
            cv.Required(CONF_DATA): validate_raw_data,
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
    if isinstance(data, bytes):
        data = [HexInt(x) for x in data]
    cg.add(var.set_data(data))
