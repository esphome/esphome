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
        # Cut to 32 bits, as the uint32_t setter always did
        if startup_delay := conf[CONF_STARTUP_DELAY].total_milliseconds & 0xFFFFFFFF:
            component_class, template_args = DelayedIntervalCallback, [startup_delay]
            trigger_class = DelayedIntervalTrigger
        else:
            component_class, template_args = IntervalCallback, []
            trigger_class = None
        var = await automation.build_component_automation(
            conf,
            component_class,
            id_key=CONF_ID,
            template_args=template_args,
            # A hand-set id keeps a trigger, which takes the delay as a constructor argument
            trigger_class=trigger_class,
            trigger_args=template_args,
        )
        cg.add(var.set_update_interval(conf[CONF_INTERVAL]))
