import logging

import esphome.codegen as cg
from esphome.components import event
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_THRESHOLD, CONF_UPDATE_INTERVAL
import esphome.final_validate as fv

from .. import CONF_MOTION_ID, MotionComponent, motion_ns

DEPENDENCIES = ["motion"]

_LOGGER = logging.getLogger(__name__)

MotionEvent = motion_ns.class_("MotionEvent", event.Event, cg.Component)

EVENT_TYPES = ["shake"]

CONF_COOLDOWN = "cooldown"

# Shake detection needs frequent samples to catch the motion pattern; a slower
# parent update_interval makes shakes likely to be missed between polls.
MAX_RECOMMENDED_UPDATE_INTERVAL_MS = 100

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
    full_config = fv.full_config.get()
    motion_path = full_config.get_path_for_id(config[CONF_MOTION_ID])[:-1]
    motion_config = full_config.get_config_for_path(motion_path)
    update_interval = motion_config[CONF_UPDATE_INTERVAL]
    if update_interval.total_milliseconds > MAX_RECOMMENDED_UPDATE_INTERVAL_MS:
        _LOGGER.warning(
            "Motion component '%s' has update_interval %s, but shake detection "
            "works best with an update_interval of %dms or less.",
            config[CONF_MOTION_ID],
            update_interval,
            MAX_RECOMMENDED_UPDATE_INTERVAL_MS,
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config):
    parent = await cg.get_variable(config[CONF_MOTION_ID])
    var = cg.new_Pvariable(config[CONF_ID], parent)
    await event.register_event(var, config, event_types=EVENT_TYPES)
    await cg.register_component(var, config)

    cg.add(var.set_threshold(config[CONF_THRESHOLD]))
    cg.add(var.set_cooldown(config[CONF_COOLDOWN]))
