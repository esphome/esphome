import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light, hbridge
from esphome.const import CONF_OUTPUT_ID

CODEOWNERS = ["@FaBjE"]
AUTO_LOAD = ["hbridge"]

HBridgeLightOutput = hbridge.hbridge_ns.class_(
    "HBridgeLightOutput", light.LightOutput, hbridge.HBridge
)

CONFIG_SCHEMA = light.RGB_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(HBridgeLightOutput),
    }
).extend(hbridge.HBRIDGE_CONFIG_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)
    await light.register_light(var, config)

    # HBridge driver config
    await hbridge.hbridge_setup(config, var)
