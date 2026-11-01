from esphome import preferences
import esphome.codegen as cg
from esphome.components import text
import esphome.config_validation as cv
from esphome.const import CONF_ID, ENTITY_CATEGORY_CONFIG
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import (  # noqa: F401  pylint: disable=unused-import
    CONF_LD2450_ID,
    CONF_POLYGON_ZONE_ID,
    CONF_POLYGON_ZONES,
    DOMAIN,
    FILTER_SOURCE_FILES,
    LD2450Component,
    ld2450_ns,
)

DEPENDENCIES = ["ld2450"]

ICON_VECTOR_POLYGON = "mdi:vector-polygon"

# Must match MIN_POLYGON_POINTS and MAX_POLYGON_POINTS in polygon.h
MIN_POLYGON_POINTS = 3
MAX_POLYGON_POINTS = 23
# Matches what Polygon::parse() accepts, apart from coordinate ranges, which only the device checks:
# empty, or MIN_POLYGON_POINTS to MAX_POLYGON_POINTS "x,y" points separated by ';'. HA validates input with it.
# No character classes: the HA frontend compiles it with the JS "v" flag, HA core with Python re.
POLYGON_PATTERN = (
    r"^ *$|^ *-?\d{1,4} *, *\d{1,4} *"
    rf"(; *-?\d{{1,4}} *, *\d{{1,4}} *){{{MIN_POLYGON_POINTS - 1},{MAX_POLYGON_POINTS - 1}}};? *$"
)
# Home Assistant's limit for a state; the longest polygon the device publishes is 252 characters
POLYGON_MAX_LENGTH = 255

# sizeof(Polygon), pinned by a static_assert in polygon.h
POLYGON_SAVED_BYTES = 94

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
        # Polygons are in raw radar mm, checked before sensor filters (polygon_zone.h)
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
    """Each polygon zone must have exactly one presence binary sensor, on the same LD2450."""
    # Polygon zone id -> the LD2450 id of each presence binary sensor that uses it
    references: dict[str, list[str]] = {}
    for binary_sensor_conf in fv.platform_entries(
        domain="binary_sensor", platform="ld2450"
    ):
        hub_id = binary_sensor_conf[CONF_LD2450_ID].id
        for presence_conf in binary_sensor_conf.get(CONF_POLYGON_ZONES, []):
            zone_id = presence_conf[CONF_POLYGON_ZONE_ID].id
            references.setdefault(zone_id, []).append(hub_id)

    zone_hub_id = config[CONF_LD2450_ID].id
    for index, zone_conf in enumerate(config[CONF_POLYGON_ZONES]):
        zone_id = zone_conf[CONF_ID].id
        hub_ids = references.get(zone_id, [])
        if len(hub_ids) != 1:
            problem = (
                "has no presence binary sensor"
                if not hub_ids
                else "is used by more than one presence binary sensor"
            )
            raise cv.Invalid(
                f"Polygon zone '{zone_id}' {problem}; each zone needs exactly one, under "
                f"'binary_sensor: - platform: ld2450' -> '{CONF_POLYGON_ZONES}'",
                path=[CONF_POLYGON_ZONES, index],
            )
        if hub_ids[0] != zone_hub_id:
            raise cv.Invalid(
                f"Polygon zone '{zone_id}' belongs to LD2450 '{zone_hub_id}', but its presence "
                f"binary sensor is under LD2450 '{hub_ids[0]}'; use the same '{CONF_LD2450_ID}'",
                path=[CONF_POLYGON_ZONES, index],
            )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    hub = await cg.get_variable(config[CONF_LD2450_ID])
    # Each zone saves its polygon; the ESP8266 checks that every component's saved values fit
    preferences.reserve_preferences(
        len(config[CONF_POLYGON_ZONES]), POLYGON_SAVED_BYTES
    )
    for zone_conf in config[CONF_POLYGON_ZONES]:
        zone = await text.new_text(
            zone_conf, max_length=POLYGON_MAX_LENGTH, pattern=POLYGON_PATTERN
        )
        _request_polygon_zone_slot(str(hub))
        cg.add(hub.register_polygon_zone(zone))
