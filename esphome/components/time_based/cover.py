from esphome import automation
import esphome.codegen as cg
from esphome.components import cover
import esphome.config_validation as cv
from esphome.const import (
    CONF_ASSUMED_STATE,
    CONF_CLOSE_ACTION,
    CONF_CLOSE_DURATION,
    CONF_OPEN_ACTION,
    CONF_OPEN_DURATION,
    CONF_STOP_ACTION,
)

time_based_ns = cg.esphome_ns.namespace("time_based")
TimeBasedCover = time_based_ns.class_("TimeBasedCover", cover.Cover, cg.Component)

CONF_HAS_BUILT_IN_ENDSTOP = "has_built_in_endstop"
CONF_MANUAL_CONTROL = "manual_control"

CONFIG_SCHEMA = (
    cover.cover_schema(TimeBasedCover)
    .extend(
        {
            cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
            cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
            cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_HAS_BUILT_IN_ENDSTOP, default=False): cv.boolean,
            cv.Optional(CONF_MANUAL_CONTROL, default=False): cv.boolean,
            cv.Optional(CONF_ASSUMED_STATE, default=True): cv.boolean,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)

    await automation.build_automation(
        var.get_stop_trigger(), [], config[CONF_STOP_ACTION]
    )

    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    await automation.build_automation(
        var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
    )

    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    await automation.build_automation(
        var.get_close_trigger(), [], config[CONF_CLOSE_ACTION]
    )

    cg.add(var.set_has_built_in_endstop(config[CONF_HAS_BUILT_IN_ENDSTOP]))
    cg.add(var.set_manual_control(config[CONF_MANUAL_CONTROL]))
    cg.add(var.set_assumed_state(config[CONF_ASSUMED_STATE]))
