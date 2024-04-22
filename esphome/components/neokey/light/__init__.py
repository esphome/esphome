import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import light
from esphome.const import CONF_OUTPUT_ID
from .. import neokey_ns, NeoKeyComponent, CONF_NEOKEY_ID

CODEOWNERS = ["@deisterhold"]

NeoKeyLight = neokey_ns.class_("NeoKeyLight", light.AddressableLight)


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(NeoKeyLight),
            cv.GenerateID(CONF_NEOKEY_ID): cv.use_id(NeoKeyComponent),
        }
    ),
    cv.only_with_arduino,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)

    hub = await cg.get_variable(config[CONF_NEOKEY_ID])
    cg.add(var.set_neokey(hub))
