from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLOSE_ACTION,
    CONF_CLOSE_DURATION,
    CONF_CLOSE_ENDSTOP,
    CONF_MAX_DURATION,
    CONF_OPEN_ACTION,
    CONF_OPEN_DURATION,
    CONF_OPEN_ENDSTOP,
    CONF_STOP_ACTION,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

endstop_ns = cg.esphome_ns.namespace("endstop")

ENDSTOP_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_STOP_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_OPEN_ENDSTOP): cv.use_id(binary_sensor.BinarySensor),
        cv.Required(CONF_OPEN_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
        cv.Required(CONF_CLOSE_ACTION): automation.validate_automation(single=True),
        cv.Required(CONF_CLOSE_ENDSTOP): cv.use_id(binary_sensor.BinarySensor),
        cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
        cv.Optional(CONF_MAX_DURATION): cv.positive_time_period_milliseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


async def setup_endstop(var: MockObj, config: ConfigType) -> None:
    """Apply the shared endstop configuration to a cover or valve platform instance."""
    await cg.register_component(var, config)

    await automation.build_automation(
        var.get_stop_trigger(), [], config[CONF_STOP_ACTION]
    )

    open_endstop = await cg.get_variable(config[CONF_OPEN_ENDSTOP])
    cg.add(var.set_open_endstop(open_endstop))
    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))
    await automation.build_automation(
        var.get_open_trigger(), [], config[CONF_OPEN_ACTION]
    )

    close_endstop = await cg.get_variable(config[CONF_CLOSE_ENDSTOP])
    cg.add(var.set_close_endstop(close_endstop))
    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))
    await automation.build_automation(
        var.get_close_trigger(), [], config[CONF_CLOSE_ACTION]
    )

    if (max_duration := config.get(CONF_MAX_DURATION)) is not None:
        cg.add(var.set_max_duration(max_duration))
