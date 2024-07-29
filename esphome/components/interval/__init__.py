import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
from esphome.const import CONF_ID, CONF_INTERVAL, CONF_STARTUP_DELAY

CODEOWNERS = ["@esphome/core"]
interval_ns = cg.esphome_ns.namespace("interval")
IntervalTrigger = interval_ns.class_(
    "IntervalTrigger", automation.Trigger.template(), cg.PollingComponent
)

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


async def to_code(config):
    for conf in config:
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await automation.build_automation(var, [], conf)

        cg.add(var.set_update_interval(conf[CONF_INTERVAL]))
        cg.add(var.set_startup_delay(conf[CONF_STARTUP_DELAY]))
