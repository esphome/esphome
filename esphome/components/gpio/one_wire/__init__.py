from esphome import pins
import esphome.codegen as cg
from esphome.components.one_wire import OneWireBus
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PIN

from .. import gpio_ns

CODEOWNERS = ["@ssieb"]

GPIOOneWireBus = gpio_ns.class_("GPIOOneWireBus", OneWireBus, cg.Component)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(GPIOOneWireBus),
        cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
