import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.const import (
    CONF_BLOCK,
    CONF_DEVICE,
    CONF_FRAGMENTATION,
    CONF_FREE,
    CONF_ID,
    CONF_LOOP_TIME,
)
from esphome.components.zephyr import (
    zephyr_add_prj_conf,
)

CODEOWNERS = ["@OttoWinter"]
DEPENDENCIES = ["logger"]

CONF_DEBUG_ID = "debug_id"
debug_ns = cg.esphome_ns.namespace("debug")
DebugComponent = debug_ns.class_("DebugComponent", cg.PollingComponent)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(DebugComponent),
            cv.Optional(CONF_DEVICE): cv.invalid(
                "The 'device' option has been moved to the 'debug' text_sensor component"
            ),
            cv.Optional(CONF_FREE): cv.invalid(
                "The 'free' option has been moved to the 'debug' sensor component"
            ),
            cv.Optional(CONF_BLOCK): cv.invalid(
                "The 'block' option has been moved to the 'debug' sensor component"
            ),
            cv.Optional(CONF_FRAGMENTATION): cv.invalid(
                "The 'fragmentation' option has been moved to the 'debug' sensor component"
            ),
            cv.Optional(CONF_LOOP_TIME): cv.invalid(
                "The 'loop_time' option has been moved to the 'debug' sensor component"
            ),
        }
    ).extend(cv.polling_component_schema("60s")),
)


async def to_code(config):
    if CORE.using_zephyr:
        zephyr_add_prj_conf("HWINFO", True)
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
