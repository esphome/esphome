import esphome.codegen as cg
from esphome.components import number
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    DEVICE_CLASS_DISTANCE,
    ENTITY_CATEGORY_CONFIG,
    ICON_TIMELAPSE,
    UNIT_MILLIMETER,
    UNIT_SECOND,
)
from esphome.types import ConfigType

from .. import CONF_LD2450_ID, LD2450Component, ld2450_ns

CONF_PRESENCE_TIMEOUT = "presence_timeout"
CONF_X1 = "x1"
CONF_X2 = "x2"
CONF_Y1 = "y1"
CONF_Y2 = "y2"
ICON_ARROW_BOTTOM_RIGHT = "mdi:arrow-bottom-right"
ICON_ARROW_BOTTOM_RIGHT_BOLD_BOX_OUTLINE = "mdi:arrow-bottom-right-bold-box-outline"
ICON_ARROW_TOP_LEFT = "mdi:arrow-top-left"
ICON_ARROW_TOP_LEFT_BOLD_BOX_OUTLINE = "mdi:arrow-top-left-bold-box-outline"
MAX_ZONES = 3

PresenceTimeoutNumber = ld2450_ns.class_("PresenceTimeoutNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_ID): cv.declare_id(cg.EntityBase),
        cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
        cv.Required(CONF_PRESENCE_TIMEOUT): number.number_schema(
            PresenceTimeoutNumber,
            unit_of_measurement=UNIT_SECOND,
            entity_category=ENTITY_CATEGORY_CONFIG,
            icon=ICON_TIMELAPSE,
        ),
    }
)

CONFIG_SCHEMA = CONFIG_SCHEMA.extend(
    {
        cv.Optional(f"zone_{n + 1}"): cv.Schema(
            {
                cv.Required(CONF_X1): number.number_schema(
                    number.ApplyNumber,
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_ARROW_TOP_LEFT_BOLD_BOX_OUTLINE,
                ),
                cv.Required(CONF_Y1): number.number_schema(
                    number.ApplyNumber,
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_ARROW_TOP_LEFT,
                ),
                cv.Required(CONF_X2): number.number_schema(
                    number.ApplyNumber,
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_ARROW_BOTTOM_RIGHT_BOLD_BOX_OUTLINE,
                ),
                cv.Required(CONF_Y2): number.number_schema(
                    number.ApplyNumber,
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_ARROW_BOTTOM_RIGHT,
                ),
            }
        )
        for n in range(MAX_ZONES)
    }
)


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2450_ID])
    numbers = number.sub_numbers(config, parent=hub)
    await numbers(
        CONF_PRESENCE_TIMEOUT,
        hub.set_presence_timeout_number,
        min_value=0,
        max_value=3600,
        step=1,
    )
    for zone_num in range(MAX_ZONES):
        if zone_conf := config.get(f"zone_{zone_num + 1}"):
            x1, y1, x2, y2 = [
                await numbers.create(
                    zone_conf[key],
                    None,
                    min_value=lo,
                    max_value=hi,
                    step=1,
                    apply="set_zone_coordinate(entity)",
                    publish="before",
                )
                for key, lo, hi in (
                    (CONF_X1, -4860, 4860),
                    (CONF_Y1, 0, 7560),
                    (CONF_X2, -4860, 4860),
                    (CONF_Y2, 0, 7560),
                )
            ]
            cg.add(hub.set_zone_numbers(zone_num, x1, y1, x2, y2))
