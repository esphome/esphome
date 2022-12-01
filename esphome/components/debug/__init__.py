import esphome.codegen as cg
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.components import logger
from esphome.const import (
    CONF_BLOCK,
    CONF_DEVICE,
    CONF_FRAGMENTATION,
    CONF_FREE,
    CONF_ID,
    CONF_LEVEL,
    CONF_LOGGER,
    CONF_LOOP_TIME,
)

CODEOWNERS = ["@OttoWinter"]
DEPENDENCIES = ["logger"]

CONF_DEBUG_ID = "debug_id"
debug_ns = cg.esphome_ns.namespace("debug")
DebugComponent = debug_ns.class_("DebugComponent", cg.PollingComponent)


CONFIG_SCHEMA = cv.Schema(
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
).extend(cv.polling_component_schema("60s"))


def _final_validate(_):
    logger_conf = fv.full_config.get()[CONF_LOGGER]
    severity = logger.LOG_LEVEL_SEVERITY.index(logger_conf[CONF_LEVEL])
    if severity < logger.LOG_LEVEL_SEVERITY.index("DEBUG"):
        raise cv.Invalid(
            "The debug component requires the logger to be at least at DEBUG level"
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
