from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERVAL, CONF_STARTUP_DELAY
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
DOMAIN = "interval"
interval_ns = cg.esphome_ns.namespace("interval")
IntervalTrigger = interval_ns.class_(
    "IntervalTrigger", automation.Trigger.template(), cg.PollingComponent
)
DelayedIntervalTrigger = interval_ns.class_("DelayedIntervalTrigger", IntervalTrigger)

CONFIG_SCHEMA = automation.validate_automation(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(IntervalTrigger),
            cv.Optional(
                CONF_STARTUP_DELAY, default="0s"
            ): cv.positive_time_period_milliseconds,
            cv.Required(CONF_INTERVAL): cv.positive_time_period_milliseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config: list[ConfigType]) -> None:
    for conf in config:
        if startup_delay := conf[CONF_STARTUP_DELAY].total_milliseconds:
            conf[CONF_ID].type = DelayedIntervalTrigger
            var = cg.new_Pvariable(conf[CONF_ID], startup_delay)
        else:
            var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await automation.build_automation(var, [], conf)

        cg.add(var.set_update_interval(conf[CONF_INTERVAL]))
