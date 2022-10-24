import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sm_10bit_base

AUTO_LOAD = ["sm_10bit_base", "output"]
CODEOWNERS = ["@Cossid"]
MULTI_CONF = True

sm2235_ns = cg.esphome_ns.namespace("sm2235")

SM2235 = sm2235_ns.class_("SM2235", sm_10bit_base.SM_10BIT_Base)

CONFIG_SCHEMA = (
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(SM2235),
        }
    )
    .extend(sm_10bit_base.SM_10BIT_BASE_CONFIG_SCHEMA)
)

async def to_code(config):
    var = await sm_10bit_base.register_sm_10bit_base(config)
    cg.add(var.set_model(0x40))
