"""Combined tests for PAGESEL/PAGESEL1 behaviour with MADCTL/PIXFMT.

Covers both the suppression behaviour (when PAGESEL or PAGESEL1 are present)
and the error behaviour when neither page-selection command is present.
"""

from __future__ import annotations

from typing import Any

import pytest

from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.components.mipi import MADCTL, PAGESEL, PAGESEL1, PIXFMT
from esphome.components.mipi_spi.display import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
import esphome.config_validation as cv
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def validated_config(config: dict[str, Any]) -> dict[str, Any]:
    """Run schema + final validation and return the validated config."""
    cfg = CONFIG_SCHEMA(config)
    FINAL_VALIDATE_SCHEMA(cfg)
    return cfg


def test_madctl_error_suppressed_when_pagesel_present(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """If PAGESEL is present in init_sequence, MADCTL presence must not raise an error."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    cfg = {
        "model": "custom",
        "dc_pin": 18,
        "dimensions": {"width": 320, "height": 240},
        "transform": {"mirror_x": True, "mirror_y": True, "swap_xy": False},
        "init_sequence": [[PAGESEL, 0x00], [MADCTL, 0x01]],
    }

    # Should not raise
    validated = validated_config(cfg)
    assert validated is not None


def test_pixfmt_error_suppressed_when_pagesel1_present(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """If PAGESEL1 is present in init_sequence, PIXFMT presence must not raise an error."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    cfg = {
        "model": "custom",
        "dc_pin": 18,
        "dimensions": {"width": 320, "height": 240},
        "init_sequence": [[PAGESEL1, 0x00], [PIXFMT, 0x01]],
    }

    # Should not raise
    validated = validated_config(cfg)
    assert validated is not None


def test_madctl_raises_without_pagesel(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """MADCTL in the init_sequence should raise when a transform is configured and
    no PAGESEL/PAGESEL1 is present.
    """
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    cfg: dict[str, Any] = {
        "model": "custom",
        "dc_pin": 18,
        "dimensions": {"width": 320, "height": 240},
        "transform": {"mirror_x": True, "mirror_y": True, "swap_xy": False},
        "init_sequence": [[MADCTL, 0x01]],
    }

    with pytest.raises(cv.Invalid, match=r"MADCTL .* in the init sequence"):
        CONFIG_SCHEMA(cfg)


def test_pixfmt_raises_without_pagesel1(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """PIXFMT in the init_sequence should raise when no PAGESEL/PAGESEL1 is present."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    cfg: dict[str, Any] = {
        "model": "custom",
        "dc_pin": 18,
        "dimensions": {"width": 320, "height": 240},
        "init_sequence": [[PIXFMT, 0x01]],
    }

    with pytest.raises(
        cv.Invalid, match=r"PIXFMT .* should not be in the init sequence"
    ):
        CONFIG_SCHEMA(cfg)
