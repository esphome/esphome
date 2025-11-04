from esphome import automation
import esphome.codegen as cg
from esphome.components import binary_sensor, cover
import esphome.config_validation as cv
from esphome.const import (
    CONF_CLOSE_DURATION,
    CONF_CLOSE_ENDSTOP,
    CONF_MAX_DURATION,
    CONF_OPEN_DURATION,
    CONF_OPEN_ENDSTOP,
)

CONF_SINGLE_BUTTON_ACTION = "single_button_action"

singlebutton_ns = cg.esphome_ns.namespace("single_button_garage_door")
SingleButtonCover = singlebutton_ns.class_(
    "SingleButtonCover", cover.Cover, cg.Component
)

CONFIG_SCHEMA = (
    cover.cover_schema(SingleButtonCover)
    .extend(
        {
            cv.Required(CONF_OPEN_ENDSTOP): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_SINGLE_BUTTON_ACTION): automation.validate_automation(
                single=True
            ),
            cv.Required(CONF_OPEN_DURATION): cv.positive_time_period_milliseconds,
            cv.Required(CONF_CLOSE_ENDSTOP): cv.use_id(binary_sensor.BinarySensor),
            cv.Required(CONF_CLOSE_DURATION): cv.positive_time_period_milliseconds,
            cv.Optional(CONF_MAX_DURATION): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


async def to_code(config):
    var = await cover.new_cover(config)
    await cg.register_component(var, config)

    await automation.build_automation(
        var.get_single_button_trigger(), [], config[CONF_SINGLE_BUTTON_ACTION]
    )

    bin = await cg.get_variable(config[CONF_OPEN_ENDSTOP])
    cg.add(var.set_open_endstop(bin))
    cg.add(var.set_open_duration(config[CONF_OPEN_DURATION]))

    bin = await cg.get_variable(config[CONF_CLOSE_ENDSTOP])
    cg.add(var.set_close_endstop(bin))
    cg.add(var.set_close_duration(config[CONF_CLOSE_DURATION]))

    if CONF_MAX_DURATION in config:
        cg.add(var.set_max_duration(config[CONF_MAX_DURATION]))
