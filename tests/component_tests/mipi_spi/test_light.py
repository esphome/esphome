"""Tests for the mipi_spi light platform validation."""

from __future__ import annotations

import pytest

from esphome import final_validate
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.components.mipi_spi.display import CONFIG_SCHEMA as DISPLAY_SCHEMA
from esphome.components.mipi_spi.light import (
    CONFIG_SCHEMA as LIGHT_SCHEMA,
    FINAL_VALIDATE_SCHEMA as LIGHT_FINAL_VALIDATE_SCHEMA,
)
from esphome.config import Config
import esphome.config_validation as cv
from esphome.const import (
    CONF_BRIGHTNESS,
    CONF_DISPLAY_ID,
    CONF_GAMMA_CORRECT,
    CONF_ID,
    CONF_MAX_BRIGHTNESS,
    CONF_MIN_BRIGHTNESS,
    PlatformFramework,
)
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

DISPLAY_ID = "test_display"

_CUSTOM: ConfigType = {
    "model": "custom",
    "dimensions": {"width": 240, "height": 240},
    "init_sequence": [[0xA0, 0x01]],
}


@pytest.fixture(autouse=True)
def _esp32(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )


def _light(**extra: object) -> ConfigType:
    return {
        "name": "Display Brightness",
        CONF_DISPLAY_ID: DISPLAY_ID,
        **extra,
    }


def _final_validate_with_display(display: ConfigType) -> None:
    """Validate a display, place it in the full config, then final-validate a light on it."""
    display = DISPLAY_SCHEMA(
        {"id": DISPLAY_ID, "dc_pin": 18, **display},
    )
    full = Config()
    full["display"] = [display]
    full.declare_ids.append((display[CONF_ID], ["display", 0, CONF_ID]))
    final_validate.full_config.set(full)
    LIGHT_FINAL_VALIDATE_SCHEMA({CONF_DISPLAY_ID: ID(DISPLAY_ID, is_declaration=False)})


def test_light_defaults() -> None:
    """Gamma defaults to 1.0 and the range to the full 0-255."""
    config = LIGHT_SCHEMA(_light())

    assert config[CONF_GAMMA_CORRECT] == 1.0
    assert config[CONF_MIN_BRIGHTNESS] == 0
    assert config[CONF_MAX_BRIGHTNESS] == 255


@pytest.mark.parametrize(
    ("min_brightness", "max_brightness"),
    [(100, 100), (200, 100)],
    ids=["equal", "inverted"],
)
def test_light_rejects_bad_range(min_brightness: int, max_brightness: int) -> None:
    with pytest.raises(
        cv.Invalid, match="'min_brightness' must be less than 'max_brightness'"
    ):
        LIGHT_SCHEMA(
            _light(min_brightness=min_brightness, max_brightness=max_brightness)
        )


@pytest.mark.parametrize("key", [CONF_MIN_BRIGHTNESS, CONF_MAX_BRIGHTNESS])
def test_light_rejects_out_of_range_value(key: str) -> None:
    with pytest.raises(cv.Invalid):
        LIGHT_SCHEMA(_light(**{key: 256}))


@pytest.mark.parametrize(
    "display",
    [
        pytest.param({"model": "rm690b0"}, id="model_with_default_brightness"),
        pytest.param({**_CUSTOM, CONF_BRIGHTNESS: 0}, id="custom_with_brightness"),
    ],
)
def test_light_accepts_display_with_brightness(display: ConfigType) -> None:
    _final_validate_with_display(display)


def test_light_rejects_model_without_brightness() -> None:
    with pytest.raises(
        cv.Invalid,
        match="Display model ILI9488 does not support setting brightness",
    ):
        _final_validate_with_display({"model": "ili9488"})


def test_light_rejects_custom_without_brightness() -> None:
    with pytest.raises(
        cv.Invalid,
        match="The 'brightness' option must be set in the display config",
    ):
        _final_validate_with_display(_CUSTOM)


def test_brightness_not_allowed_on_model_without_default() -> None:
    with pytest.raises(cv.Invalid, match=r"extra keys not allowed.*brightness"):
        DISPLAY_SCHEMA(
            {"id": DISPLAY_ID, "dc_pin": 18, "model": "ili9488", CONF_BRIGHTNESS: 10}
        )
