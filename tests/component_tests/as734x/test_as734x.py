"""Tests for the AS734x typed schema.

The AS7341 and AS7343 branches differ in exactly two ways: the AS7343 offers two extra gain steps
and three extra bands. A YAML test can only show that a valid config is accepted, so the rejection
side of that split is covered here.
"""

from typing import Any

import pytest

import esphome.config_validation as cv


def _config(**overrides: Any) -> dict[str, Any]:
    config: dict[str, Any] = {"type": "AS7341", "id": "test_as734x"}
    config.update(overrides)
    return config


@pytest.mark.parametrize(
    ("gain", "match"),
    [
        ("X1024", "X1024"),
        ("X2048", "X2048"),
    ],
)
def test_as7341_rejects_as7343_only_gains(gain: str, match: str) -> None:
    """X1024 and X2048 exist only on the AS7343."""
    from esphome.components.as734x.sensor import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match=match):
        CONFIG_SCHEMA(_config(gain=gain))


@pytest.mark.parametrize("band", ["fz", "fy", "fxl"])
def test_as7341_rejects_as7343_only_bands(band: str) -> None:
    """FZ, FY and FXL exist only on the AS7343."""
    from esphome.components.as734x.sensor import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match=band):
        CONFIG_SCHEMA(_config(counts={band: "Band"}))


@pytest.mark.parametrize("gain", ["X0.5", "X512", "X1024", "X2048"])
def test_as7343_accepts_its_full_gain_range(gain: str) -> None:
    from esphome.components.as734x.sensor import CONFIG_SCHEMA

    config = CONFIG_SCHEMA(_config(type="AS7343", gain=gain))
    assert config["gain"] is not None


@pytest.mark.parametrize("band", ["f1", "fz", "fy", "fxl", "nir", "clear"])
def test_as7343_accepts_its_full_band_set(band: str) -> None:
    from esphome.components.as734x.sensor import CONFIG_SCHEMA

    config = CONFIG_SCHEMA(_config(type="AS7343", counts={band: "Band"}))
    assert band in config["counts"]


@pytest.mark.parametrize("gain", ["X0.5", "X8", "X512"])
def test_as7341_accepts_its_own_gain_range(gain: str) -> None:
    from esphome.components.as734x.sensor import CONFIG_SCHEMA

    config = CONFIG_SCHEMA(_config(gain=gain))
    assert config["gain"] is not None
