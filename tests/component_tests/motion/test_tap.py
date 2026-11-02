"""Tests for the shared motion tap schema and set_tap_threshold validation."""

from __future__ import annotations

import pytest

from esphome.components.const import CONF_ON_DOUBLE_TAP, CONF_ON_TAP, CONF_TAP
from esphome.components.motion import (
    AXES,
    CONF_AXES,
    KEY_TAP_THRESHOLD_IDS,
    _final_validate,
    tap_schema,
    validate_axes,
)
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_THRESHOLD
from esphome.core import CORE, ID


def test_tap_defaults() -> None:
    """Threshold takes the platform default and all axes are watched."""
    config = tap_schema("0.6g")({CONF_ON_TAP: []})
    assert config[CONF_THRESHOLD] == pytest.approx(0.6)
    assert config[CONF_AXES] == AXES


def test_tap_needs_a_trigger() -> None:
    """A tap block with neither trigger does nothing, so it is rejected."""
    with pytest.raises(cv.Invalid, match="at least one of"):
        tap_schema("0.6g")({})


def test_tap_accepts_both_triggers() -> None:
    """Single and double taps can be watched together."""
    config = tap_schema("0.6g")({CONF_ON_TAP: [], CONF_ON_DOUBLE_TAP: []})
    assert CONF_ON_TAP in config
    assert CONF_ON_DOUBLE_TAP in config


def test_tap_threshold_takes_g() -> None:
    """The threshold is in g, with or without the unit."""
    assert tap_schema("0.6g")({CONF_THRESHOLD: "1.5g", CONF_ON_TAP: []})[
        CONF_THRESHOLD
    ] == pytest.approx(1.5)
    with pytest.raises(cv.Invalid):
        tap_schema("0.6g")({CONF_THRESHOLD: "-1g", CONF_ON_TAP: []})


def test_tap_extra_keys_are_platform_only() -> None:
    """A platform's own options are accepted only when it adds them."""
    extra = {cv.Optional("window", default="250ms"): cv.positive_time_period}
    assert "window" in tap_schema("0.6g", extra)({CONF_ON_TAP: []})
    with pytest.raises(cv.Invalid):
        tap_schema("0.6g")({"window": "250ms", CONF_ON_TAP: []})


def test_axes_reject_duplicates() -> None:
    """An axis listed twice is a mistake."""
    assert validate_axes(["X", "z"]) == ["x", "z"]
    with pytest.raises(cv.Invalid, match="only once"):
        validate_axes(["x", "x"])


@pytest.mark.parametrize("has_tap", [True, False])
def test_set_tap_threshold_needs_tap_block(
    has_tap: bool, monkeypatch: pytest.MonkeyPatch
) -> None:
    """The threshold setter is compiled out without a tap block."""
    monkeypatch.setitem(CORE.data, KEY_TAP_THRESHOLD_IDS, {"accel"})
    conf = {CONF_ID: ID("accel")}
    if has_tap:
        conf[CONF_TAP] = {}
        _final_validate([{CONF_ID: ID("other")}, conf])
    else:
        with pytest.raises(cv.Invalid, match="needs a 'tap' block") as err:
            _final_validate([{CONF_ID: ID("other")}, conf])
        assert err.value.path == [1, CONF_ID]


def test_unreferenced_sensor_needs_no_tap_block() -> None:
    """A sensor no threshold action targets needs no tap block."""
    _final_validate([{CONF_ID: ID("other")}])
