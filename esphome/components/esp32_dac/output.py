from esphome import pins
from esphome.components import output
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import CONF_ID, CONF_NUMBER, CONF_PIN

DEPENDENCIES = ["esp32"]


def valid_dac_pin(value):
    num = value[CONF_NUMBER]
    cv.one_of(25, 26)(num)
    return value


esp32_dac_ns = cg.esphome_ns.namespace("esp32_dac")
ESP32DAC = esp32_dac_ns.class_("ESP32DAC", output.FloatOutput, cg.Component)

CONFIG_SCHEMA = output.FLOAT_OUTPUT_SCHEMA.extend(
    {
        cv.Required(CONF_ID): cv.declare_id(ESP32DAC),
        cv.Required(CONF_PIN): cv.All(
            pins.internal_gpio_output_pin_schema, valid_dac_pin
        ),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    await output.register_output(var, config)

    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
