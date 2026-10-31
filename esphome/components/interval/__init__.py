from functools import partial

from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERVAL, CONF_STARTUP_DELAY
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

CODEOWNERS = ["@esphome/core"]
DOMAIN = "interval"
interval_ns = cg.esphome_ns.namespace("interval")
IntervalTrigger = interval_ns.class_(
    "IntervalTrigger", automation.Trigger.template(), cg.PollingComponent
)
IntervalCallback = interval_ns.class_("IntervalCallback", cg.PollingComponent)
DelayedIntervalCallback = interval_ns.class_(
    "DelayedIntervalCallback", IntervalCallback
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


async def to_code(config: list[ConfigType]) -> None:
    for conf in config:
        if startup_delay := conf[CONF_STARTUP_DELAY].total_milliseconds:
            component_class, template_args = DelayedIntervalCallback, [startup_delay]
        else:
            component_class, template_args = IntervalCallback, []
        var = await automation.build_component_automation(
            conf,
            component_class,
            id_key=CONF_ID,
            template_args=template_args,
            # A hand-set id keeps the IntervalTrigger, which takes the delay at run time
            trigger_setup=partial(_set_startup_delay, startup_delay)
            if startup_delay
            else None,
        )
        cg.add(var.set_update_interval(conf[CONF_INTERVAL]))


def _set_startup_delay(startup_delay: int, trigger: MockObj) -> None:
    cg.add(trigger.set_startup_delay(startup_delay))
