import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_ID,
    CONF_READ_PIN,
    CONF_WRITE_PIN,
)

CODEOWNERS = ["@khenderick"]

CONF_OPENTHERM_ID = "opentherm_id"

opentherm = cg.esphome_ns.namespace("opentherm")
OpenThermComponent = opentherm.class_("OpenThermComponent", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(OpenThermComponent),
        cv.Required(CONF_READ_PIN): pins.gpio_input_pin_schema,
        cv.Required(CONF_WRITE_PIN): pins.gpio_output_pin_schema,
    }
).extend(cv.polling_component_schema("5s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    read_pin = await cg.gpio_pin_expression(config[CONF_READ_PIN])
    write_pin = await cg.gpio_pin_expression(config[CONF_WRITE_PIN])
    cg.add(var.set_pins(read_pin, write_pin))
