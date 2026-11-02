import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID

CODEOWNERS = ["@esphome/tests"]

scheduler_loop_phase_component_ns = cg.esphome_ns.namespace(
    "scheduler_loop_phase_component"
)
SchedulerLoopPhaseComponent = scheduler_loop_phase_component_ns.class_(
    "SchedulerLoopPhaseComponent", cg.Component
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SchedulerLoopPhaseComponent),
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
