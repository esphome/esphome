from esphome import automation
import esphome.codegen as cg
from esphome.components import valve
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLOSE_ACTION,
    CONF_DURATION,
    CONF_OPEN_ACTION,
    CONF_RESTORE_MODE,
    CONF_STOP_ACTION,
)

from .. import time_based_ns

TimeBasedValve = time_based_ns.class_("TimeBasedValve", valve.Valve, cg.Component)

TimeBasedValveRestoreMode = time_based_ns.enum("TimeBasedValveRestoreMode")
RESTORE_MODES = {
    "NO_RESTORE": TimeBasedValveRestoreMode.VALVE_NO_RESTORE,
    "RESTORE": TimeBasedValveRestoreMode.VALVE_RESTORE,
    "ALWAYS_OPEN": TimeBasedValveRestoreMode.VALVE_ALWAYS_OPEN,
    "ALWAYS_CLOSED": TimeBasedValveRestoreMode.VALVE_ALWAYS_CLOSED,
}

CONFIG_SCHEMA = (
    valve.valve_schema(TimeBasedValve)
    .extend(
        {
            cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_RESTORE_MODE, default="NO_RESTORE"): cv.enum(
                RESTORE_MODES, upper=True
            ),
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
    cg.add(var.set_restore_mode(config[CONF_RESTORE_MODE]))
