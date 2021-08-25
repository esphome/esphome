from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID, CONF_PIN
from .. import status_led_ns

StatusLEDLightOutput = status_led_ns.class_(
    "StatusLEDLightOutput", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = light.BINARY_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(StatusLEDLightOutput),
        cv.Required(CONF_PIN): pins.gpio_output_pin_schema,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    pin = await cg.gpio_pin_expression(config[CONF_PIN])
    cg.add(var.set_pin(pin))
    await cg.register_component(var, config)
    # cg.add(cg.App.register_component(var))
    await light.register_light(var, config)
