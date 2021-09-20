from esphome.const import CONF_ID
import esphome.codegen as cg
import esphome.config_validation as cv

CODEOWNERS = ["@esphome/core"]

preferences_ns = cg.esphome_ns.namespace("preferences")
IntervalSyncer = preferences_ns.class_("IntervalSyncer", cg.Component)

CONF_FLASH_WRITE_INTERVAL = "flash_write_interval"
CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(IntervalSyncer),
        cv.Optional(
            CONF_FLASH_WRITE_INTERVAL, default="60s"
        ): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    cg.add(var.set_write_interval(config[CONF_FLASH_WRITE_INTERVAL]))
    await cg.register_component(var, config)
