"""Tests for the ld2450 xy_unfiltered button."""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.ld2450 import CONF_LD2450_ID
from esphome.components.ld2450.button import CONF_XY_UNFILTERED, _final_validate
from esphome.const import CONF_PLATFORM
from esphome.core import ID

HUB = ID("radar", is_declaration=False)
BUTTON = {CONF_LD2450_ID: HUB, CONF_XY_UNFILTERED: {}}


@pytest.mark.parametrize(
    "text_sensors",
    [[], [{CONF_PLATFORM: "ld2450", CONF_LD2450_ID: HUB, "version": {}}]],
)
def test_xy_unfiltered_needs_an_xy_sensor(
    set_component_config: Callable[[str, Any], None], text_sensors: list
) -> None:
    set_component_config("text_sensor", text_sensors)
    with pytest.raises(cv.Invalid, match="needs an ld2450 text sensor with xy"):
        _final_validate(BUTTON)


def test_xy_unfiltered_with_an_xy_sensor(
    set_component_config: Callable[[str, Any], None],
) -> None:
    set_component_config(
        "text_sensor",
        [{CONF_PLATFORM: "ld2450", CONF_LD2450_ID: HUB, "target_2": {"xy": {}}}],
    )
    _final_validate(BUTTON)
