import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID, CONF_PIN_A, CONF_PIN_B, CONF_ENABLE_PIN
from .. import hbridge_ns, HBRIDGE_CONFIG_SCHEMA, hbridge_config_to_code

CODEOWNERS = ["@DotNetDann"]
AUTO_LOAD = ["hbridge"]

HBridgeLightOutput = hbridge_ns.class_(
    "HBridgeLightOutput", cg.PollingComponent, light.LightOutput
)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(HBridgeLightOutput),
    }
).extend(HBRIDGE_CONFIG_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    # HBridge driver config
    await hbridge_config_to_code(config, var)