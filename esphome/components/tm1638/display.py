import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import display
from esphome.const import (
    CONF_ID,
    CONF_INTENSITY,
    CONF_LAMBDA,
    CONF_CLK_PIN,
    CONF_DIO_PIN,
    CONF_STB_PIN,
)

CODEOWNERS = ["@skykingjwc"]

CONF_TM1638_ID = "tm1638_id"

tm1638_ns = cg.esphome_ns.namespace("tm1638")
TM1638Component = tm1638_ns.class_("TM1638Component", cg.PollingComponent)
TM1638ComponentRef = TM1638Component.operator("ref")


CONFIG_SCHEMA = display.BASIC_DISPLAY_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(TM1638Component),
        cv.Required(CONF_CLK_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_STB_PIN): pins.gpio_output_pin_schema,
        cv.Required(CONF_DIO_PIN): pins.gpio_output_pin_schema,
        cv.Optional(CONF_INTENSITY, default=7): cv.int_range(min=0, max=8),
    }
).extend(cv.polling_component_schema("1s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await display.register_display(var, config)

    clk = await cg.gpio_pin_expression(config[CONF_CLK_PIN])
    cg.add(var.set_clk_pin(clk))

    dio = await cg.gpio_pin_expression(config[CONF_DIO_PIN])
    cg.add(var.set_dio_pin(dio))

    stb = await cg.gpio_pin_expression(config[CONF_STB_PIN])
    cg.add(var.set_stb_pin(stb))

    cg.add(var.set_intensity(config[CONF_INTENSITY]))

    if CONF_LAMBDA in config:
        lambda_ = await cg.process_lambda(
            config[CONF_LAMBDA], [(TM1638ComponentRef, "it")], return_type=cg.void
        )

    cg.add(var.set_writer(lambda_))
