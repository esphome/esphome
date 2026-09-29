"""Tests for the `color` option in per-item effect color lists."""

from __future__ import annotations

import pytest

from esphome import config_validation as cv
from esphome.components.light.effects import (
    ADDRESSABLE_COLOR_WIPE_COLOR_SCHEMA,
    STROBE_COLOR_SCHEMA,
)
from esphome.const import CONF_BLUE, CONF_COLOR_BRIGHTNESS, CONF_GREEN, CONF_RED


def test_strobe_plain_defaults_full_level() -> None:
    result = STROBE_COLOR_SCHEMA({"duration": "1s"})
    assert (result[CONF_RED], result[CONF_GREEN], result[CONF_BLUE]) == (1.0, 1.0, 1.0)
    assert result[CONF_COLOR_BRIGHTNESS] == 1.0


def test_strobe_color_name() -> None:
    result = STROBE_COLOR_SCHEMA({"duration": "1s", "color": "darkred"})
    assert "color" not in result
    assert (result[CONF_RED], result[CONF_GREEN], result[CONF_BLUE]) == (1.0, 0.0, 0.0)
    assert result[CONF_COLOR_BRIGHTNESS] == pytest.approx(0x8B / 0xFF)


def test_strobe_hex_color() -> None:
    result = STROBE_COLOR_SCHEMA({"duration": "1s", "color": "0x223344"})
    assert result[CONF_BLUE] == 1.0
    assert result[CONF_COLOR_BRIGHTNESS] == pytest.approx(0x44 / 0xFF)


def test_strobe_explicit_channels_unaffected() -> None:
    result = STROBE_COLOR_SCHEMA({"duration": "1s", "red": "50%"})
    assert result[CONF_RED] == 0.5
    assert (result[CONF_GREEN], result[CONF_BLUE]) == (1.0, 1.0)
    assert result[CONF_COLOR_BRIGHTNESS] == 1.0


def test_strobe_color_conflicts_with_rgb() -> None:
    with pytest.raises(cv.Invalid, match="cannot be used with"):
        STROBE_COLOR_SCHEMA({"duration": "1s", "color": "red", "red": "10%"})


def test_addressable_color_wipe_plain_defaults_full_level() -> None:
    result = ADDRESSABLE_COLOR_WIPE_COLOR_SCHEMA({"num_leds": 1})
    assert (result[CONF_RED], result[CONF_GREEN], result[CONF_BLUE]) == (1.0, 1.0, 1.0)


def test_addressable_color_wipe_color_name() -> None:
    result = ADDRESSABLE_COLOR_WIPE_COLOR_SCHEMA({"num_leds": 1, "color": "darkred"})
    assert "color" not in result
    assert (result[CONF_RED], result[CONF_GREEN], result[CONF_BLUE]) == (1.0, 0.0, 0.0)


def test_addressable_color_wipe_color_conflicts_with_rgb() -> None:
    with pytest.raises(cv.Invalid, match="cannot be used with"):
        ADDRESSABLE_COLOR_WIPE_COLOR_SCHEMA(
            {"num_leds": 1, "color": "blue", "blue": "10%"}
        )
