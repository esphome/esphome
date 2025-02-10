import esphome.codegen as cg
from esphome.components import select
import esphome.config_validation as cv
from esphome.const import CONF_LEVEL, CONF_LOGGER, ENTITY_CATEGORY_CONFIG, ICON_BUG
from esphome.core import CORE
from esphome.cpp_helpers import register_component, register_parented

from .. import CONF_LOGGER_ID, LOG_LEVEL_SEVERITY, Logger, logger_ns

CODEOWNERS = ["@clydebarrow"]

LoggerLevelSelect = logger_ns.class_("LoggerLevelSelect", select.Select, cg.Component)

CONFIG_SCHEMA = select.select_schema(
    LoggerLevelSelect, icon=ICON_BUG, entity_category=ENTITY_CATEGORY_CONFIG
).extend(
    {
        cv.GenerateID(CONF_LOGGER_ID): cv.use_id(Logger),
    }
)


async def to_code(config):
    levels = LOG_LEVEL_SEVERITY
    index = levels.index(CORE.config[CONF_LOGGER][CONF_LEVEL])
    levels = levels[: index + 1]
    var = await select.new_select(config, options=levels)
    await register_parented(var, config[CONF_LOGGER_ID])
    await register_component(var, config)
