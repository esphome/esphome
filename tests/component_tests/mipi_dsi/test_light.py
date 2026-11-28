"""Tests for the mipi_dsi brightness option and light platform validation."""

from __future__ import annotations

import pytest

from esphome import final_validate
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32P4
from esphome.components.mipi import DriverChip
from esphome.components.mipi_dsi import display as dsi_display
from esphome.components.mipi_dsi.light import (
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
from esphome.schema_extractors import SCHEMA_EXTRACT
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

DISPLAY_ID = "test_display"
AMOLED_MODEL = "TEST-AMOLED"

_CUSTOM: ConfigType = {
    "model": "custom",
    "dimensions": {"width": 480, "height": 800},
    "init_sequence": [[0xA0, 0x01]],
    "lane_bit_rate": "1Gbps",
}


@pytest.fixture(autouse=True)
def _esp32p4(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-p4-evboard", KEY_VARIANT: VARIANT_ESP32P4},
    )


@pytest.fixture
def amoled_model(monkeypatch: pytest.MonkeyPatch) -> None:
    """Add a model with a default brightness, without leaving it registered for other tests."""
    chip = dsi_display.MODELS["JC1060P470"].extend(AMOLED_MODEL, brightness=0xC0)
    DriverChip.models.pop(AMOLED_MODEL, None)
    monkeypatch.setitem(dsi_display.MODELS, AMOLED_MODEL, chip)


def _display(config: ConfigType) -> ConfigType:
    return dsi_display.CONFIG_SCHEMA({"id": DISPLAY_ID, **config})


def _light(**extra: object) -> ConfigType:
    return {"name": "Display Brightness", CONF_DISPLAY_ID: DISPLAY_ID, **extra}


def _final_validate_with_display(display: ConfigType) -> None:
    """Validate a display, place it in the full config, then final-validate a light on it."""
    display = _display(display)
    full = Config()
    full["display"] = [display]
    full.declare_ids.append((display[CONF_ID], ["display", 0, CONF_ID]))
    final_validate.full_config.set(full)
    LIGHT_FINAL_VALIDATE_SCHEMA({CONF_DISPLAY_ID: ID(DISPLAY_ID, is_declaration=False)})


def test_light_defaults() -> None:
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


def test_model_default_brightness(amoled_model: None) -> None:
    assert _display({"model": AMOLED_MODEL})[CONF_BRIGHTNESS] == 0xC0
    _final_validate_with_display({"model": AMOLED_MODEL})


def test_light_accepts_custom_with_brightness_zero() -> None:
    assert _display({**_CUSTOM, CONF_BRIGHTNESS: 0})[CONF_BRIGHTNESS] == 0
    _final_validate_with_display({**_CUSTOM, CONF_BRIGHTNESS: 0})


def test_light_rejects_custom_without_brightness() -> None:
    with pytest.raises(
        cv.Invalid, match="The 'brightness' option must be set in the display config"
    ):
        _final_validate_with_display(_CUSTOM)


def test_light_rejects_model_without_brightness() -> None:
    with pytest.raises(
        cv.Invalid, match="Display model JC1060P470 does not support setting brightness"
    ):
        _final_validate_with_display({"model": "JC1060P470"})


def test_brightness_rejected_on_model_without_default() -> None:
    with pytest.raises(
        cv.Invalid, match="Display model JC1060P470 does not support setting brightness"
    ):
        _display({"model": "JC1060P470", CONF_BRIGHTNESS: 10})


def test_schema_extraction_includes_brightness() -> None:
    schema = dsi_display.CONFIG_SCHEMA(SCHEMA_EXTRACT)
    assert CONF_BRIGHTNESS in {str(key) for key in schema.schema}
