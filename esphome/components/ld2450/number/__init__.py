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
ZoneCoordinateNumber = ld2450_ns.class_("ZoneCoordinateNumber", number.Number)

CONFIG_SCHEMA = cv.Schema(
    {
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
                    ZoneCoordinateNumber,
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_ARROW_TOP_LEFT_BOLD_BOX_OUTLINE,
                ),
                cv.Required(CONF_Y1): number.number_schema(
                    ZoneCoordinateNumber,
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_ARROW_TOP_LEFT,
                ),
                cv.Required(CONF_X2): number.number_schema(
                    ZoneCoordinateNumber,
                    device_class=DEVICE_CLASS_DISTANCE,
                    unit_of_measurement=UNIT_MILLIMETER,
                    entity_category=ENTITY_CATEGORY_CONFIG,
                    icon=ICON_ARROW_BOTTOM_RIGHT_BOLD_BOX_OUTLINE,
                ),
                cv.Required(CONF_Y2): number.number_schema(
                    ZoneCoordinateNumber,
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


async def to_code(config):
    ld2450_component = await cg.get_variable(config[CONF_LD2450_ID])
    if presence_timeout_config := config.get(CONF_PRESENCE_TIMEOUT):
        n = await number.new_number(
            presence_timeout_config,
            min_value=0,
            max_value=3600,
            step=1,
        )
        await cg.register_parented(n, config[CONF_LD2450_ID])
        cg.add(ld2450_component.set_presence_timeout_number(n))
    for x in range(MAX_ZONES):
        if zone_conf := config.get(f"zone_{x + 1}"):
            if zone_x1_config := zone_conf.get(CONF_X1):
                n = cg.new_Pvariable(zone_x1_config[CONF_ID], x)
                await number.register_number(
                    n, zone_x1_config, min_value=-4860, max_value=4860, step=1
                )
                await cg.register_parented(n, config[CONF_LD2450_ID])
                cg.add(ld2450_component.set_zone_x1_number(x, n))
            if zone_y1_config := zone_conf.get(CONF_Y1):
                n = cg.new_Pvariable(zone_y1_config[CONF_ID], x)
                await number.register_number(
                    n, zone_y1_config, min_value=0, max_value=7560, step=1
                )
                await cg.register_parented(n, config[CONF_LD2450_ID])
                cg.add(ld2450_component.set_zone_y1_number(x, n))
            if zone_x2_config := zone_conf.get(CONF_X2):
                n = cg.new_Pvariable(zone_x2_config[CONF_ID], x)
                await number.register_number(
                    n, zone_x2_config, min_value=-4860, max_value=4860, step=1
                )
                await cg.register_parented(n, config[CONF_LD2450_ID])
                cg.add(ld2450_component.set_zone_x2_number(x, n))
            if zone_y2_config := zone_conf.get(CONF_Y2):
                n = cg.new_Pvariable(zone_y2_config[CONF_ID], x)
                await number.register_number(
                    n, zone_y2_config, min_value=0, max_value=7560, step=1
                )
                await cg.register_parented(n, config[CONF_LD2450_ID])
                cg.add(ld2450_component.set_zone_y2_number(x, n))
