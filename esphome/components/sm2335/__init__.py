import esphome.codegen as cg
from esphome.components import sm10bit_base
import esphome.config_validation as cv

AUTO_LOAD = ["sm10bit_base", "output"]
CODEOWNERS = ["@Cossid"]
MULTI_CONF = True

sm2335_ns = cg.esphome_ns.namespace("sm2335")

SM2335 = sm2335_ns.class_("SM2335", sm10bit_base.Sm10BitBase)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SM2335),
    }
).extend(sm10bit_base.SM10BIT_BASE_CONFIG_SCHEMA)


async def to_code(config):
    var = await sm10bit_base.register_sm10bit_base(config)
    cg.add(var.set_model(0xC0))
