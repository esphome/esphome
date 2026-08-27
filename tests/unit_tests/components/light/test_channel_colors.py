"""Tests for the shared addressable-strip channel order helpers."""

import logging

import pytest

from esphome.components.const import CONF_CHANNEL_COLORS, CONF_IS_WRGB
from esphome.components.light import (
    channel_colors_struct,
    migrate_channel_colors,
    validate_channel_colors,
)
import esphome.config_validation as cv
from esphome.const import CONF_IS_RGBW, CONF_RGB_ORDER
from esphome.types import ConfigType

NO_WHITE = "light::ChannelColors::NO_WHITE"


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("RGB", "RGB"),
        ("grb", "GRB"),
        ("BRG", "BRG"),
        ("rgbw", "RGBW"),
        ("WRGB", "WRGB"),
        ("GWRB", "GWRB"),
    ],
)
def test_validate_channel_colors(value: str, expected: str) -> None:
    assert validate_channel_colors(value) == expected


@pytest.mark.parametrize(
    "value",
    [
        "RG",  # missing a channel
        "RGBB",  # duplicate channel
        "RRGB",  # duplicate channel, correct length
        "RGBWW",  # two white channels
        "RGBX",  # unknown channel
        "RGBWX",  # unknown channel, correct length
        "",
    ],
)
def test_validate_channel_colors_rejects_invalid(value: str) -> None:
    with pytest.raises(cv.Invalid, match="is not a valid channel order"):
        validate_channel_colors(value)


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        ("RGB", (0, 1, 2, NO_WHITE)),
        ("GRB", (1, 0, 2, NO_WHITE)),
        ("BRG", (1, 2, 0, NO_WHITE)),
        ("RGBW", (0, 1, 2, 3)),
        ("GRBW", (1, 0, 2, 3)),
        ("WRGB", (1, 2, 3, 0)),
        ("GWRB", (2, 0, 3, 1)),
    ],
)
def test_channel_colors_struct(value: str, expected: tuple[int, int, int, int]) -> None:
    struct = channel_colors_struct(value)
    assert str(struct.base) == "light::ChannelColors"
    assert tuple(str(arg) for arg in struct.args.values()) == tuple(
        str(field) for field in expected
    )


def _migrate(config: ConfigType) -> ConfigType:
    return migrate_channel_colors(removed_in="2027.3.0", component="test_strip")(config)


def test_migrate_passes_through_channel_colors() -> None:
    config = {CONF_CHANNEL_COLORS: "GRBW"}
    assert _migrate(config) == {CONF_CHANNEL_COLORS: "GRBW"}


@pytest.mark.parametrize(
    ("deprecated", "expected", "named"),
    [
        ({}, "GRB", "'rgb_order' is"),
        (
            {CONF_IS_RGBW: False, CONF_IS_WRGB: False},
            "GRB",
            "'rgb_order', 'is_rgbw' and 'is_wrgb' are",
        ),
        ({CONF_IS_RGBW: True}, "GRBW", "'rgb_order' and 'is_rgbw' are"),
        ({CONF_IS_WRGB: True}, "WGRB", "'rgb_order' and 'is_wrgb' are"),
    ],
)
def test_migrate_folds_deprecated_keys(
    deprecated: ConfigType,
    expected: str,
    named: str,
    caplog: pytest.LogCaptureFixture,
) -> None:
    config = {CONF_RGB_ORDER: "GRB", "num_leds": 1, **deprecated}
    with caplog.at_level(logging.WARNING):
        result = _migrate(config)

    assert result == {CONF_CHANNEL_COLORS: expected, "num_leds": 1}
    assert f"[test_strip] {named} deprecated" in caplog.text
    assert f"'{CONF_CHANNEL_COLORS}: {expected}'" in caplog.text
    assert "2027.3.0" in caplog.text


def test_migrate_does_not_mutate_input() -> None:
    config = {CONF_RGB_ORDER: "GRB", CONF_IS_RGBW: True}
    _migrate(config)
    assert config == {CONF_RGB_ORDER: "GRB", CONF_IS_RGBW: True}


@pytest.mark.parametrize("deprecated", [CONF_RGB_ORDER, CONF_IS_RGBW, CONF_IS_WRGB])
def test_migrate_rejects_mixing_old_and_new(deprecated: str) -> None:
    config = {CONF_CHANNEL_COLORS: "GRBW", deprecated: "GRB"}
    with pytest.raises(cv.Invalid, match=f"cannot be combined with '{deprecated}'"):
        _migrate(config)


def test_migrate_reports_every_conflicting_key() -> None:
    config = {
        CONF_CHANNEL_COLORS: "GRBW",
        CONF_RGB_ORDER: "GRB",
        CONF_IS_RGBW: True,
        CONF_IS_WRGB: False,
    }
    with pytest.raises(
        cv.Invalid, match="cannot be combined with 'rgb_order', 'is_rgbw' and 'is_wrgb'"
    ):
        _migrate(config)


def test_migrate_requires_channel_colors() -> None:
    with pytest.raises(cv.Invalid, match=f"'{CONF_CHANNEL_COLORS}' is required"):
        _migrate({"num_leds": 1})


def test_migrate_rejects_is_rgbw_with_is_wrgb() -> None:
    config = {CONF_RGB_ORDER: "GRB", CONF_IS_RGBW: True, CONF_IS_WRGB: True}
    with pytest.raises(cv.Invalid, match="cannot both be enabled"):
        _migrate(config)
