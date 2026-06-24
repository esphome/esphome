import esphome.codegen as cg
from esphome.components import light, output
import esphome.config_validation as cv
from esphome.const import CONF_OUTPUT_ID, CONF_PIN_A, CONF_PIN_B, CONF_UPDATE_INTERVAL

from .. import hbridge_ns

CODEOWNERS = ["@DotNetDann"]

HBridgeLightOutput = hbridge_ns.class_(
    "HBridgeLightOutput", cg.PollingComponent, light.LightOutput
)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(HBridgeLightOutput),
        cv.Required(CONF_PIN_A): cv.use_id(output.FloatOutput),
        cv.Required(CONF_PIN_B): cv.use_id(output.FloatOutput),
        # Sets how often the H-bridge direction is flipped to multiplex cold/warm white.
        cv.Optional(CONF_UPDATE_INTERVAL, default="8ms"): cv.update_interval,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    # Apply update_interval to the polling output directly and pop it, so light.register_light
    # (which registers the non-polling LightState with this same config) does not try to.
    cg.add(var.set_update_interval(config.pop(CONF_UPDATE_INTERVAL)))
    await cg.register_component(var, config)
    await light.register_light(var, config)

    hside = await cg.get_variable(config[CONF_PIN_A])
    cg.add(var.set_pina_pin(hside))
    lside = await cg.get_variable(config[CONF_PIN_B])
    cg.add(var.set_pinb_pin(lside))
