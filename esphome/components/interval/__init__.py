from esphome import automation
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_INTERVAL, CONF_STARTUP_DELAY

CODEOWNERS = ["@esphome/core"]
CONF_AUTOSTART = "autostart"
CONF_START_CONDITIONS = "start_conditions"

interval_ns = cg.esphome_ns.namespace("interval")
IntervalTrigger = interval_ns.class_(
    "IntervalTrigger", automation.Trigger.template(), cg.PollingComponent
)

# Actions
StartIntervalAction = interval_ns.class_(
    "IntervalStartAction", automation.Action, cg.Parented.template(IntervalTrigger)
)
StopIntervalAction = interval_ns.class_(
    "IntervalStopAction", automation.Action, cg.Parented.template(IntervalTrigger)
)

CONFIG_SCHEMA = automation.validate_automation(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(IntervalTrigger),
            cv.Exclusive(
                CONF_STARTUP_DELAY, CONF_START_CONDITIONS
            ): cv.positive_time_period_milliseconds,
            cv.Exclusive(CONF_AUTOSTART, CONF_START_CONDITIONS): cv.boolean,
            cv.Required(CONF_INTERVAL): cv.positive_time_period_milliseconds,
        },
    ).extend(cv.COMPONENT_SCHEMA)
)

START_INTERVAL_SCHEMA = automation.maybe_simple_id(
    {
        CONF_ID: cv.use_id(IntervalTrigger),
    }
)

STOP_INTERVAL_SCHEMA = automation.maybe_simple_id(
    {
        CONF_ID: cv.use_id(IntervalTrigger),
    }
)


@automation.register_action(
    "interval.start", StartIntervalAction, START_INTERVAL_SCHEMA
)
@automation.register_action("interval.stop", StopIntervalAction, STOP_INTERVAL_SCHEMA)
async def interval_action_to_code(config, action_id, template_arg, args):
    parent = await cg.get_variable(config[CONF_ID])
    var = cg.new_Pvariable(action_id, template_arg, parent)
    return var


async def to_code(config):
    for conf in config:
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await automation.build_automation(var, [], conf)

        cg.add(var.set_update_interval(conf[CONF_INTERVAL]))
        cg.add(var.set_autostart(conf.get(CONF_AUTOSTART, True)))
        cg.add(var.set_startup_delay(conf.get(CONF_STARTUP_DELAY, 0)))
