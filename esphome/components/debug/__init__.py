import esphome.config_validation as cv
import esphome.codegen as cg
from esphome.const import (
    CONF_ID,
)

CODEOWNERS = ["@OttoWinter"]
DEPENDENCIES = ["logger"]

CONF_DEBUG_ID = "debug_id"
debug_ns = cg.esphome_ns.namespace("debug")
DebugComponent = debug_ns.class_("DebugComponent", cg.PollingComponent)


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(DebugComponent),
    }
).extend(cv.polling_component_schema("60s"))


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
