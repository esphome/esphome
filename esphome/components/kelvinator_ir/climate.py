import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import climate_ir
from esphome.const import CONF_ID

AUTO_LOAD = ["climate_ir"]
CODEOWNERS = ["@michael-horne"]

CONF_HEADER_LIGHT = "light"

kelvinator_ir_ns = cg.esphome_ns.namespace("kelvinator_ir")
KelvinatorIR = kelvinator_ir_ns.class_("KelvinatorIR", climate_ir.ClimateIR)

CONFIG_SCHEMA = climate_ir.CLIMATE_IR_WITH_RECEIVER_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(KelvinatorIR),
        cv.Optional(CONF_HEADER_LIGHT, default=True): cv.boolean,
    }
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await climate_ir.register_climate_ir(var, config)
    cg.add(var.set_light(config[CONF_HEADER_LIGHT]))
