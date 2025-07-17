import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

scheduler_string_lifetime_component_ns = cg.esphome_ns.namespace(
    "scheduler_string_lifetime_component"
)
SchedulerStringLifetimeComponent = scheduler_string_lifetime_component_ns.class_(
    "SchedulerStringLifetimeComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SchedulerStringLifetimeComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
