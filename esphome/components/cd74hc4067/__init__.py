import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@asoehlke", "rmorenoramos"]
AUTO_LOAD = ["sensor", "voltage_sampler"]

cd74hc4067_ns = cg.esphome_ns.namespace("cd74hc4067")

CD74HC4067Component = cd74hc4067_ns.class_(
    "CD74HC4067Component", cg.Component, cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(CD74HC4067Component),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
