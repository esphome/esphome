from esphome import automation
import esphome.codegen as cg
from esphome.components import valve
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLOSE_ACTION,
    CONF_DURATION,
    CONF_OPEN_ACTION,
    CONF_STOP_ACTION,
)

time_based_ns = cg.esphome_ns.namespace("time_based")
TimeBasedValve = time_based_ns.class_("TimeBasedValve", valve.Valve, cg.Component)

CONFIG_SCHEMA = (
    valve.valve_schema(TimeBasedValve)
    .extend(
        {
            cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_DURATION): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await valve.new_valve(config)
    await cg.register_component(var, config)

    await automation.build_automation(
        var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
    )

    await automation.build_automation(
        var.get_close_trigger(), [], config[CONF_CLOSE_ACTION]
    )

    await automation.build_automation(
        var.get_stop_trigger(), [], config[CONF_STOP_ACTION]
    )

    cg.add(var.set_duration(config[CONF_DURATION]))
