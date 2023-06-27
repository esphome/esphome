from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_OUTPUT, CONF_OUTPUT_ID, CONF_PIN
from .. import status_led_ns

AUTO_LOAD = ["output"]

StatusLEDLightOutput = status_led_ns.class_(
    "StatusLEDLightOutput", light.LightOutput, cg.Component
)

CONFIG_SCHEMA = cv.All(
    light.BINARY_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(StatusLEDLightOutput),
            cv.Optional(CONF_PIN): pins.gpio_output_pin_schema,
            cv.Optional(CONF_OUTPUT): cv.use_id(output.BinaryOutput),
        }
    ),
    cv.has_at_least_one_key(CONF_PIN, CONF_OUTPUT),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    if CONF_PIN in config:
        pin = await cg.gpio_pin_expression(config[CONF_PIN])
        cg.add(var.set_pin(pin))
    if CONF_OUTPUT in config:
        out = await cg.get_variable(config[CONF_OUTPUT])
        cg.add(var.set_output(out))
    await cg.register_component(var, config)
    await light.register_light(var, config)
