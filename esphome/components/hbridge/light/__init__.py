import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, output
from esphome.const import CONF_OUTPUT_ID, CONF_PIN_A, CONF_PIN_B
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
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    hside = await cg.get_variable(config[CONF_PIN_A])
    cg.add(var.set_pina_pin(hside))
    lside = await cg.get_variable(config[CONF_PIN_B])
    cg.add(var.set_pinb_pin(lside))
