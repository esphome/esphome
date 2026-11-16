"""Tests for the camera_decoder platform and variant restrictions."""

import pytest

from esphome.components.camera_decoder import CONFIG_SCHEMA
from esphome.components.esp32 import (
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32H4,
    VARIANT_ESP32H21,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
)
import esphome.config_validation as cv
from esphome.const import CONF_ESPHOME, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable

# esp_new_jpeg 1.0.2 ships a prebuilt library for these targets.
SUPPORTED_VARIANTS = [
    VARIANT_ESP32,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H4,
    VARIANT_ESP32P4,
]

# No esp_new_jpeg build exists for these, so they are rejected at config time.
UNSUPPORTED_VARIANTS = [VARIANT_ESP32H2, VARIANT_ESP32H21]


@pytest.mark.parametrize(
    ("platform_framework", "variant"),
    [(PlatformFramework.ESP32_IDF, variant) for variant in SUPPORTED_VARIANTS]
    + [(PlatformFramework.ESP32_ARDUINO, variant) for variant in SUPPORTED_VARIANTS],
)
def test_supported_variants(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
    variant: str,
) -> None:
    """Every variant with an esp_new_jpeg build validates on both frameworks."""
    set_core_config(
        platform_framework,
        platform_data={KEY_VARIANT: variant},
        full_config={CONF_ESPHOME: {}},
    )

    config = CONFIG_SCHEMA({})

    assert config["output_type"] is not None
    assert config["rotation"] == 0


@pytest.mark.parametrize("variant", UNSUPPORTED_VARIANTS)
def test_unsupported_variants_rejected(
    set_core_config: SetCoreConfigCallable, variant: str
) -> None:
    """ESP32-H2 and ESP32-H21 are rejected before dependency resolution."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_VARIANT: variant},
        full_config={CONF_ESPHOME: {}},
    )

    with pytest.raises(cv.Invalid, match="is not available on"):
        CONFIG_SCHEMA({})


def test_non_esp32_rejected(set_core_config: SetCoreConfigCallable) -> None:
    """The decoder is ESP32 only."""
    set_core_config(PlatformFramework.RP2040_ARDUINO, full_config={CONF_ESPHOME: {}})

    with pytest.raises(cv.Invalid, match="only available on"):
        CONFIG_SCHEMA({})
