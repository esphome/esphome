import esphome.codegen as cg
from esphome.components import text
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PLATFORM, ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import (
    CONF_LD2450_ID,
    CONF_POLYGON_ZONE_ID,
    CONF_POLYGON_ZONES,
    LD2450Component,
    ld2450_ns,
)

DEPENDENCIES = ["ld2450"]

ICON_VECTOR_POLYGON = "mdi:vector-polygon"

# Must match MIN_POLYGON_POINTS and MAX_POLYGON_POINTS in polygon.h
MIN_POLYGON_POINTS = 3
MAX_POLYGON_POINTS = 16
# Matches what Polygon::parse() accepts, apart from coordinate ranges, which only the device checks:
# empty, or MIN_POLYGON_POINTS to MAX_POLYGON_POINTS "x,y" points separated by ';'. HA validates input with it.
# No character classes: the HA frontend compiles it with the JS "v" flag, HA core with Python re.
POLYGON_PATTERN = (
    r"^ *$|^ *-?\d{1,4} *, *\d{1,4} *"
    rf"(; *-?\d{{1,4}} *, *\d{{1,4}} *){{{MIN_POLYGON_POINTS - 1},{MAX_POLYGON_POINTS - 1}}};? *$"
)
# Longest polygon the device publishes: "-4860,7560" (10 characters) per point, ';' between points
POLYGON_MAX_LENGTH = MAX_POLYGON_POINTS * 11 - 1

PolygonZone = ld2450_ns.class_("PolygonZone", text.Text)

_request_polygon_zone_slot = cg.slot_counter("LD2450_POLYGON_ZONE_COUNT")


def _require_manual_id(config: ConfigType) -> ConfigType:
    if not config[CONF_ID].is_manual:
        raise cv.Invalid(
            f"'{CONF_ID}' is required, so the zone's presence binary sensor can refer to it",
            path=[CONF_ID],
        )
    return config


CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_LD2450_ID): cv.use_id(LD2450Component),
        cv.Required(CONF_POLYGON_ZONES): cv.ensure_list(
            text.text_schema(
                PolygonZone,
                entity_category=ENTITY_CATEGORY_CONFIG,
                icon=ICON_VECTOR_POLYGON,
                mode="TEXT",
            ),
            _require_manual_id,
        ),
    }
)


def _final_validate(config: ConfigType) -> None:
    """Each polygon zone must have exactly one presence binary sensor."""
    references: dict[str, int] = {}
    for binary_sensor_conf in fv.full_config.get().get("binary_sensor", []):
        if binary_sensor_conf.get(CONF_PLATFORM) != "ld2450":
            continue
        for presence_conf in binary_sensor_conf.get(CONF_POLYGON_ZONES, []):
            zone_id = presence_conf[CONF_POLYGON_ZONE_ID].id
            references[zone_id] = references.get(zone_id, 0) + 1

    for index, zone_conf in enumerate(config[CONF_POLYGON_ZONES]):
        zone_id = zone_conf[CONF_ID].id
        count = references.get(zone_id, 0)
        if count == 1:
            continue
        problem = (
            "has no presence binary sensor"
            if count == 0
            else "is used by more than one presence binary sensor"
        )
        raise cv.Invalid(
            f"Polygon zone '{zone_id}' {problem}; each zone needs exactly one, under "
            f"'binary_sensor: - platform: ld2450' -> '{CONF_POLYGON_ZONES}'",
            path=[CONF_POLYGON_ZONES, index],
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2450_ID])
    for zone_conf in config[CONF_POLYGON_ZONES]:
        zone = await text.new_text(
            zone_conf, max_length=POLYGON_MAX_LENGTH, pattern=POLYGON_PATTERN
        )
        _request_polygon_zone_slot()
        cg.add(hub.register_polygon_zone(zone))
