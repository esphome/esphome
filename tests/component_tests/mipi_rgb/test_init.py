"""Tests for mipi_rgb configuration validation, in particular the per-model
``requires`` component check (see esphome.components.mipi.DriverChip.check_requirements)."""

from __future__ import annotations

from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32S3
from esphome.components.mipi_rgb.display import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA

# Importing pca9554 registers its pin schema with pins.PIN_SCHEMA_REGISTRY so that
# models (e.g. SEEED-INDICATOR-D1) that reference pca9554-backed pins in their
# defaults can be validated by the mipi_rgb CONFIG_SCHEMA in this test.
import esphome.components.pca9554  # noqa: F401
from esphome.const import PlatformFramework
from esphome.core import CORE
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _validated(config: ConfigType) -> ConfigType:
    """Run the component config schema followed by the final validation."""
    config = CONFIG_SCHEMA(config)
    FINAL_VALIDATE_SCHEMA(config)
    return config


def test_model_requires_psram(set_core_config: SetCoreConfigCallable) -> None:
    """A model known to have PSRAM on its board rejects a config without it.

    RGB parallel displays always need a full framebuffer, so every model in this
    component is expected to carry ``requires={"psram", ...}``. This board has no
    other requirements, so its check is exercised in isolation here.
    """
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    CORE.raw_config = {}

    with pytest.raises(
        cv.Invalid,
        match=r"ESP32-8048S070 requires component 'psram' to be configured",
    ):
        _validated({"model": "ESP32-8048S070"})


def test_model_requires_psram_satisfied(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Any,
) -> None:
    """The same board model validates once PSRAM is configured."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    set_component_config("psram", True)
    CORE.raw_config = {"psram": True}

    config = _validated({"model": "ESP32-8048S070"})
    assert config["model"] == "ESP32-8048S070"


def test_model_requires_psram_and_expander(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Any,
) -> None:
    """A model that also depends on an I2C GPIO expander lists both when missing."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    # Only satisfy one of the two requirements.
    set_component_config("psram", True)
    CORE.raw_config = {"psram": True}

    with pytest.raises(
        cv.Invalid,
        match=r"SEEED-INDICATOR-D1 requires component 'pca9554' to be configured",
    ):
        _validated(
            {
                "model": "SEEED-INDICATOR-D1",
                "spi_id": "spi_bus",
            }
        )
