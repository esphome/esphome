import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import CONF_THRESHOLD, ENTITY_CATEGORY_CONFIG, ICON_PULSE
from esphome.types import ConfigType

from . import (
    CONF_ESPECTRE_ID,
    CONF_MOTION_OFF_HITS,
    CONF_MOTION_ON_HITS,
    MOTION_HITS_MAX,
    MOTION_HITS_MIN,
    ESPectreComponent,
)

DEPENDENCIES = ["espectre"]


def _config_number_schema(icon: str) -> cv.Schema:
    return number.number_schema(
        number.ApplyNumber, entity_category=ENTITY_CATEGORY_CONFIG, icon=icon
    )


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_ESPECTRE_ID): cv.use_id(ESPectreComponent),
            cv.Optional(CONF_THRESHOLD): _config_number_schema(ICON_PULSE),
            cv.Optional(CONF_MOTION_ON_HITS): _config_number_schema(
                "mdi:motion-play-outline"
            ),
            cv.Optional(CONF_MOTION_OFF_HITS): _config_number_schema(
                "mdi:motion-pause-outline"
            ),
        }
    ),
    cv.has_at_least_one_key(CONF_THRESHOLD, CONF_MOTION_ON_HITS, CONF_MOTION_OFF_HITS),
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_ESPECTRE_ID])
    numbers = number.sub_numbers(config, parent=hub)
    # The hub publishes each value once the runtime has applied it.
    await numbers(
        CONF_THRESHOLD,
        hub.set_threshold_number,
        min_value=0.0,
        max_value=1.0,
        step=0.01,
        apply="request_threshold(value)",
    )
    hits = {"min_value": MOTION_HITS_MIN, "max_value": MOTION_HITS_MAX, "step": 1}
    await numbers(
        CONF_MOTION_ON_HITS,
        hub.set_motion_on_hits_number,
        **hits,
        apply="request_motion_on_hits(value)",
    )
    await numbers(
        CONF_MOTION_OFF_HITS,
        hub.set_motion_off_hits_number,
        **hits,
        apply="request_motion_off_hits(value)",
    )
