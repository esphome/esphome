import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sm_10bit_base

AUTO_LOAD = ["sm_10bit_base", "output"]
CODEOWNERS = ["@Cossid"]
MULTI_CONF = True

sm2335_ns = cg.esphome_ns.namespace("sm2335")

SM2335 = sm2335_ns.class_("SM2335", sm_10bit_base.SM_10BIT_Base)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SM2335),
    }
).extend(sm_10bit_base.SM_10BIT_BASE_CONFIG_SCHEMA)


async def to_code(config):
    var = await sm_10bit_base.register_sm_10bit_base(config)
    cg.add(var.set_model(0xC0))
