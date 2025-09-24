from esphome import pins
import esphome.codegen as cg
from esphome.components import lcd_base
import esphome.config_validation as cv
from esphome.const import (
    CONF_DATA_PINS,
    CONF_ENABLE_PIN,
    CONF_ID,
    CONF_LAMBDA,
    CONF_RS_PIN,
    CONF_RW_PIN,
)

AUTO_LOAD = ["lcd_base"]

lcd_gpio_ns = cg.esphome_ns.namespace("lcd_gpio")
GPIOLCDDisplay = lcd_gpio_ns.class_("GPIOLCDDisplay", lcd_base.LCDDisplay)


def validate_pin_length(value):
    if len(value) != 4 and len(value) != 8:
        raise cv.Invalid(
            f"LCD Displays can either operate in 4-pin or 8-pin mode,not {len(value)}-pin mode"
        )
    return value


CONFIG_SCHEMA = lcd_base.LCD_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(GPIOLCDDisplay),
        cv.Required(CONF_DATA_PINS): cv.All(
            [pins.gpio_output_pin_schema], validate_pin_length
        ),
        cv.Required(CONF_ENABLE_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_RS_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_RW_PIN): pins.gpio_output_pin_schema,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await lcd_base.setup_lcd_display(var, config)
    pins_ = []
    for conf in config[CONF_DATA_PINS]:
        pins_.append(await cg.gpio_pin_expression(conf))
    cg.add(var.set_data_pins(*pins_))
    enable = await cg.gpio_pin_expression(config[CONF_ENABLE_PIN])
    cg.add(var.set_enable_pin(enable))

    rs = await cg.gpio_pin_expression(config[CONF_RS_PIN])
    cg.add(var.set_rs_pin(rs))

    if CONF_RW_PIN in config:
        rw = await cg.gpio_pin_expression(config[CONF_RW_PIN])
        cg.add(var.set_rw_pin(rw))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA],
            [(GPIOLCDDisplay.operator("ref"), "it")],
            return_type=cg.void,
        )
        cg.add(var.set_writer(lambda_))
