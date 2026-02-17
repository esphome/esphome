import esphome.codegen as cg
from esphome.components import display, text_display, uart
import esphome.config_validation as cv
from esphome.const import CONF_DIMENSIONS, CONF_ID, CONF_LAMBDA

AUTO_LOAD = ["text_display"]


uart_terminal = cg.esphome_ns.namespace("uart_terminal")
Terminal = uart_terminal.class_("Terminal", text_display.TextDisplay)


def validate_dimensions(value):
    value = cv.dimensions(value)
    if value[0] > 255:
        raise cv.Invalid("Displays can't have more than 255 columns")
    if value[1] > 255:
        raise cv.Invalid("Displays can't have more than 255 rows")
    return value


CONFIG_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(Terminal),
        cv.Required(CONF_DIMENSIONS): validate_dimensions,
    }
).extend(uart.UART_DEVICE_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await display.register_display(var, config)
    await uart.register_uart_device(var, config)
    cg.add(var.set_dimensions(config[CONF_DIMENSIONS][0], config[CONF_DIMENSIONS][1]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(Terminal.operator("ref"), "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
