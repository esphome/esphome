import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import number
from esphome.const import (
    CONF_ID,
    CONF_CS_PIN,
    CONF_INC_PIN,
    CONF_UD_PIN,
    CONF_INITIAL_VALUE,
)

CODEOWNERS = ["@EtienneMD"]

x9c_ns = cg.esphome_ns.namespace("x9c")

X9cNumber = x9c_ns.class_("X9cNumber", number.Number, cg.Component)

CONFIG_SCHEMA = cv.All(
    number.NUMBER_SCHEMA.extend(
        {
            cv.GenerateID(): cv.declare_id(X9cNumber),
            cv.Required(CONF_CS_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_INC_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_UD_PIN): pins.internal_gpio_output_pin_schema,
            cv.Optional(CONF_INITIAL_VALUE, default=100): cv.int_range(min=1, max=100),
        }
    )
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await number.register_number(
        var,
        config,
        min_value=1,
        max_value=100,
        step=1,
    )

    cs_pin = await cg.gpio_pin_expression(config[CONF_CS_PIN])
    cg.add(var.set_cs_pin(cs_pin))
    inc_pin = await cg.gpio_pin_expression(config[CONF_INC_PIN])
    cg.add(var.set_inc_pin(inc_pin))
    ud_pin = await cg.gpio_pin_expression(config[CONF_UD_PIN])
    cg.add(var.set_ud_pin(ud_pin))

    cg.add(var.set_initial_value(config[CONF_INITIAL_VALUE]))
