"""Tests for the ld2450 polygon zone validation.

Polygon zones are declared in the text platform and their presence sensors in the
binary sensor platform, so the pairing between the two is checked at final validation.
"""

from __future__ import annotations

import pytest

from esphome.components.ld2450 import CONF_POLYGON_ZONE_ID, CONF_POLYGON_ZONES
from esphome.components.ld2450.text import (
    CONFIG_SCHEMA as TEXT_CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA as TEXT_FINAL_VALIDATE_SCHEMA,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_NAME, CONF_PLATFORM, PlatformFramework
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

HUB_ID = "ld2450_hub"


def _text_config(*zone_ids: str) -> ConfigType:
    """A text platform config declaring the given polygon zones."""
    return {
        "ld2450_id": ID(HUB_ID, is_declaration=False, type="ld2450"),
        CONF_POLYGON_ZONES: [
            {CONF_ID: ID(zone_id, is_declaration=True), CONF_NAME: zone_id}
            for zone_id in zone_ids
        ],
    }


def _binary_sensors(*zone_ids: str) -> list[ConfigType]:
    """A binary sensor platform list with one presence sensor per given zone reference."""
    return [
        {
            CONF_PLATFORM: "ld2450",
            CONF_POLYGON_ZONES: [
                {CONF_POLYGON_ZONE_ID: ID(zone_id, is_declaration=False)}
                for zone_id in zone_ids
            ],
        }
    ]


def test_zones_paired_with_one_presence_pass(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={"binary_sensor": _binary_sensors("couch", "desk")},
    )

    TEXT_FINAL_VALIDATE_SCHEMA(_text_config("couch", "desk"))


def test_zone_without_presence_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={"binary_sensor": _binary_sensors("couch")},
    )

    with pytest.raises(
        cv.Invalid,
        match=r"^Polygon zone 'desk' has no presence binary sensor; .* @ data\['polygon_zones'\]\[1\]$",
    ):
        TEXT_FINAL_VALIDATE_SCHEMA(_text_config("couch", "desk"))


def test_zone_without_any_binary_sensor_platform_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config={})

    with pytest.raises(cv.Invalid, match="'couch' has no presence binary sensor"):
        TEXT_FINAL_VALIDATE_SCHEMA(_text_config("couch"))


def test_zone_with_two_presences_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Two references to one zone are rejected, even across binary sensor platform entries."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={
            "binary_sensor": _binary_sensors("couch") + _binary_sensors("couch")
        },
    )

    with pytest.raises(
        cv.Invalid,
        match=r"^Polygon zone 'couch' is used by more than one presence binary sensor; ",
    ):
        TEXT_FINAL_VALIDATE_SCHEMA(_text_config("couch"))


def test_other_platforms_are_ignored(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Binary sensors of other platforms never count as presence for a zone."""
    other = {CONF_PLATFORM: "template", CONF_POLYGON_ZONES: []}
    set_core_config(
        PlatformFramework.ESP32_IDF,
        full_config={"binary_sensor": [other, *_binary_sensors("couch")]},
    )

    TEXT_FINAL_VALIDATE_SCHEMA(_text_config("couch"))


def test_zone_without_id_is_rejected(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """The presence sensor refers to the zone by id, so an automatic id is no use."""
    set_core_config(PlatformFramework.ESP32_IDF)

    with pytest.raises(cv.Invalid, match=r"^'id' is required, so the zone's presence"):
        TEXT_CONFIG_SCHEMA({CONF_POLYGON_ZONES: [{CONF_NAME: "Couch Zone Polygon"}]})
