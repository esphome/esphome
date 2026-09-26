"""Tests for the light `color` option."""

from __future__ import annotations

import logging

import pytest

from esphome import config_validation as cv
from esphome.components.light.automation import (
    LIGHT_CONTROL_ACTION_SCHEMA,
    LIGHT_STATE_SCHEMA,
)
from esphome.const import (
    CONF_BLUE,
    CONF_COLOR_BRIGHTNESS,
    CONF_GREEN,
    CONF_ID,
    CONF_RED,
)

LOGGER_NAME = "esphome.components.light.automation"


def test_color_name_sets_rgb() -> None:
    result = LIGHT_STATE_SCHEMA({"color": "Tomato"})
    assert "color" not in result
    assert result[CONF_RED] == 1.0
    assert result[CONF_GREEN] == pytest.approx(0x63 / 0xFF)
    assert result[CONF_BLUE] == pytest.approx(0x47 / 0xFF)
    assert result[CONF_COLOR_BRIGHTNESS] == 1.0


def test_color_name_in_control_action() -> None:
    result = LIGHT_CONTROL_ACTION_SCHEMA({CONF_ID: "test_light", "color": "blue"})
    assert (result[CONF_RED], result[CONF_GREEN], result[CONF_BLUE]) == (0, 0, 1.0)


def test_unknown_color_name() -> None:
    with pytest.raises(cv.Invalid, match="notacolor"):
        LIGHT_STATE_SCHEMA({"color": "notacolor"})


def test_color_name_conflicts_with_rgb() -> None:
    with pytest.raises(cv.Invalid, match="cannot be used with"):
        LIGHT_STATE_SCHEMA({"color": "red", CONF_GREEN: 0.5})


def test_dark_color_sets_color_brightness() -> None:
    result = LIGHT_STATE_SCHEMA({"color": "darkred"})
    assert result[CONF_RED] == 1.0
    assert result[CONF_GREEN] == 0.0
    assert result[CONF_BLUE] == 0.0
    assert result[CONF_COLOR_BRIGHTNESS] == pytest.approx(0x8B / 0xFF)


def test_black_sets_zero_color_brightness() -> None:
    result = LIGHT_STATE_SCHEMA({"color": "black"})
    assert result[CONF_COLOR_BRIGHTNESS] == 0.0
    assert (result[CONF_RED], result[CONF_GREEN], result[CONF_BLUE]) == (0, 0, 0)


def test_explicit_color_brightness_wins(caplog: pytest.LogCaptureFixture) -> None:
    with caplog.at_level(logging.WARNING, logger=LOGGER_NAME):
        result = LIGHT_STATE_SCHEMA({"color": "darkred", CONF_COLOR_BRIGHTNESS: 0.25})
    assert result[CONF_COLOR_BRIGHTNESS] == 0.25
    assert "overrides the brightness of color 'darkred'" in caplog.text


def test_explicit_color_brightness_full_color(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level(logging.WARNING, logger=LOGGER_NAME):
        result = LIGHT_STATE_SCHEMA({"color": "red", CONF_COLOR_BRIGHTNESS: 0.25})
    assert result[CONF_COLOR_BRIGHTNESS] == 0.25
    assert not [r for r in caplog.records if r.name == LOGGER_NAME]
