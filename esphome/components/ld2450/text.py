from dataclasses import dataclass
import logging

import esphome.codegen as cg
from esphome.components import text
from esphome.components.esp8266.const import CONF_RESTORE_FROM_FLASH
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PLATFORM, ENTITY_CATEGORY_CONFIG
from esphome.core import CORE
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import (
    CONF_LD2450_ID,
    CONF_POLYGON_ZONE_ID,
    CONF_POLYGON_ZONES,
    DOMAIN,
    LD2450Component,
    ld2450_ns,
)

DEPENDENCIES = ["ld2450"]

_LOGGER = logging.getLogger(__name__)

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

# ESP8266 space for saved settings, in 4-byte words. Must match esp8266/preferences.cpp: RTC memory has two regions
# (RTC_NORMAL_REGION_WORDS, RTC_EBOOT_REGION_WORDS) that one saved value can't span; with restore_from_flash,
# everything goes to flash (ESP8266_FLASH_STORAGE_SIZE)
ESP8266_RTC_REGION_WORDS = (78, 32)
ESP8266_FLASH_WORDS = 128
# A saved Polygon (its points, count and padding), plus one checksum word
POLYGON_ZONE_SAVED_WORDS = -(-(MAX_POLYGON_POINTS * 4 + 2) // 4) + 1

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
        # Polygons are in the radar's raw coordinates (mm). Target sensor filters only change published values,
        # so a target X/Y sensor with a filter that changes its value (multiply, offset, lambda, ...) is no
        # longer in the same frame as the polygons; filters that only delay or smooth values keep the frame.
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


@dataclass
class _TextData:
    esp8266_space_checked: bool = False


def _get_data() -> _TextData:
    if DOMAIN not in CORE.data:
        CORE.data[DOMAIN] = _TextData()
    return CORE.data[DOMAIN]


def _check_esp8266_space(full_config: ConfigType) -> None:
    """Check that every polygon zone, of every LD2450, fits in the ESP8266's space for saved settings.

    A zone that doesn't fit can never be saved, so it could never be set.
    """
    data = _get_data()
    if not CORE.is_esp8266 or data.esp8266_space_checked:
        return
    data.esp8266_space_checked = True

    zones = sum(
        len(text_conf[CONF_POLYGON_ZONES])
        for text_conf in full_config.get("text", [])
        if text_conf.get(CONF_PLATFORM) == "ld2450"
    )
    in_flash = full_config.get("esp8266", {}).get(CONF_RESTORE_FROM_FLASH, False)
    regions = (ESP8266_FLASH_WORDS,) if in_flash else ESP8266_RTC_REGION_WORDS
    max_zones = sum(words // POLYGON_ZONE_SAVED_WORDS for words in regions)
    where = "in flash ('restore_from_flash: true')" if in_flash else "in RTC memory"
    if zones > max_zones:
        hint = (
            "" if in_flash else ", or set 'restore_from_flash: true' under 'esp8266:'"
        )
        raise cv.Invalid(
            f"{zones} polygon zones don't fit in the ESP8266's space for saved settings {where}: at most "
            f"{max_zones} fit, and the others could never be saved; use fewer zones{hint}"
        )
    if zones == max_zones:
        _LOGGER.warning(
            "%d polygon zones fill the ESP8266's space for saved settings %s; with other saved settings, some "
            "zones may not be saved, and then can't be set",
            zones,
            where,
        )


def _final_validate(config: ConfigType) -> None:
    """Each polygon zone must have exactly one presence binary sensor, on the same LD2450."""
    full_config = fv.full_config.get()
    _check_esp8266_space(full_config)

    # Polygon zone id -> the LD2450 id of each presence binary sensor that uses it
    references: dict[str, list[str]] = {}
    for binary_sensor_conf in full_config.get("binary_sensor", []):
        if binary_sensor_conf.get(CONF_PLATFORM) != "ld2450":
            continue
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
    for zone_conf in config[CONF_POLYGON_ZONES]:
        zone = await text.new_text(
            zone_conf, max_length=POLYGON_MAX_LENGTH, pattern=POLYGON_PATTERN
        )
        _request_polygon_zone_slot(str(hub))
        cg.add(hub.register_polygon_zone(zone))
