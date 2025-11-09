import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

defer_stress_component_ns = cg.esphome_ns.namespace("defer_stress_component")
DeferStressComponent = defer_stress_component_ns.class_(
    "DeferStressComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DeferStressComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
