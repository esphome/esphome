from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@fornellas"]
MULTI_CONF = True

CONF_IN_PIN = "in_pin"
CONF_OUT_PIN = "out_pin"

opentherm42_ns = cg.esphome_ns.namespace("opentherm42")
OpenTherm42Hub = opentherm42_ns.class_("OpenTherm42Hub", cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OpenTherm42Hub),
        cv.Required(CONF_IN_PIN): pins.internal_gpio_input_pin_schema,
        cv.Required(CONF_OUT_PIN): pins.internal_gpio_output_pin_schema,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config: dict) -> None:
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    in_pin = await cg.gpio_pin_expression(config[CONF_IN_PIN])
    cg.add(var.set_in_pin(in_pin))
    out_pin = await cg.gpio_pin_expression(config[CONF_OUT_PIN])
    cg.add(var.set_out_pin(out_pin))
