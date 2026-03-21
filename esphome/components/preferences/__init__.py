import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.core import coroutine_with_priority
from esphome.coroutine import CoroPriority

CODEOWNERS = ["@esphome/core"]

preferences_ns = cg.esphome_ns.namespace("preferences")
IntervalSyncer = preferences_ns.class_("IntervalSyncer", cg.Component)

CONF_FLASH_WRITE_INTERVAL = "flash_write_interval"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(IntervalSyncer),
        cv.Optional(CONF_FLASH_WRITE_INTERVAL, default="60s"): cv.update_interval,
    }
).extend(cv.COMPONENT_SCHEMA)


@coroutine_with_priority(CoroPriority.PREFERENCES)
async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    write_interval = config[CONF_FLASH_WRITE_INTERVAL]
    if write_interval.total_milliseconds == 0:
        cg.add_define("USE_PREFERENCES_SYNC_EVERY_LOOP")
    else:
        cg.add(var.set_write_interval(write_interval))
    await cg.register_component(var, config)
