import esphome.codegen as cg
from esphome.components import text_display, uart
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_LAMBDA
from esphome.types import ConfigType

AUTO_LOAD = ["text_display"]
DEPENDENCIES = ["uart"]

uart_terminal_ns = cg.esphome_ns.namespace("uart_terminal")
Terminal = uart_terminal_ns.class_(
    "Terminal", text_display.TextDisplay, uart.UARTDevice
)

CONFIG_SCHEMA = text_display.TEXT_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(Terminal),
    }
).extend(uart.UART_DEVICE_SCHEMA)

FINAL_VALIDATE_SCHEMA = uart.final_validate_device_schema(
    "uart_terminal", require_tx=True
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await text_display.setup_text_display(var, config)
    await uart.register_uart_device(var, config)

    if (lambda_config := config.get(CONF_LAMBDA)) is not None:
        lambda_ = await cg.process_lambda(
            lambda_config, [(Terminal.operator("ref"), "it")], return_type=cg.void
        )
        cg.add(var.set_writer(lambda_))
