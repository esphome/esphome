import esphome.codegen as cg
from esphome.components import event
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_THRESHOLD

from .. import (
    CONF_MOTION_ID,
    MotionComponent,
    check_has_accelerometer,
    check_update_interval,
    motion_ns,
)

DEPENDENCIES = ["motion"]

MotionEvent = motion_ns.class_("MotionEvent", event.Event, cg.Component)

EVENT_TYPES = ["shake"]

CONF_COOLDOWN = "cooldown"

CONFIG_SCHEMA = (
    event.event_schema(MotionEvent)
    .extend(
        {
            cv.GenerateID(CONF_MOTION_ID): cv.use_id(MotionComponent),
            cv.Optional(CONF_THRESHOLD, default=0.5): cv.positive_float,
            cv.Optional(
                CONF_COOLDOWN, default="500ms"
            ): cv.positive_time_period_milliseconds,
        }
    )
    .extend(cv.COMPONENT_SCHEMA)
)


def _final_validate(config: dict) -> None:
    check_update_interval(config[CONF_MOTION_ID], "shake")
    check_has_accelerometer(config[CONF_MOTION_ID], "shake")


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MOTION_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await event.register_event(var, config, event_types=EVENT_TYPES)
    await cg.register_component(var, config)

    cg.add(var.set_threshold(config[CONF_THRESHOLD]))
    cg.add(var.set_cooldown(config[CONF_COOLDOWN]))
