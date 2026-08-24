"""Tests for SPI PSRAM DMA configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)
from esphome.components.spi import CONF_PSRAM_DMA, spi_device_schema
from esphome.const import KEY_FRAMEWORK_VERSION, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def _schema() -> cv.Schema:
    return spi_device_schema(
        cs_pin_required=False,
        default_data_rate="1MHz",
        default_mode="MODE0",
    )


def _stage(
    set_core_config: SetCoreConfigCallable,
    platform_framework: PlatformFramework,
    variant: str,
    version: cv.Version,
) -> None:
    set_core_config(
        platform_framework,
        core_data={KEY_FRAMEWORK_VERSION: version},
        platform_data={KEY_BOARD: "test-board", KEY_VARIANT: variant},
    )


def test_psram_dma_accepts_supported_idf_target(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _stage(
        set_core_config,
        PlatformFramework.ESP32_IDF,
        VARIANT_ESP32S3,
        cv.Version(5, 5, 3),
    )

    config = _schema()({CONF_PSRAM_DMA: True})

    assert config[CONF_PSRAM_DMA] is True


def test_psram_dma_rejects_arduino(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _stage(
        set_core_config,
        PlatformFramework.ESP32_ARDUINO,
        VARIANT_ESP32S3,
        cv.Version(5, 5, 3),
    )

    with pytest.raises(cv.Invalid, match="only available with framework"):
        _schema()({CONF_PSRAM_DMA: True})


def test_psram_dma_rejects_target_without_capability(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _stage(
        set_core_config,
        PlatformFramework.ESP32_IDF,
        VARIANT_ESP32,
        cv.Version(5, 5, 3),
    )

    with pytest.raises(cv.Invalid, match="PSRAM DMA is only available"):
        _schema()({CONF_PSRAM_DMA: True})


def test_psram_dma_rejects_older_idf(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _stage(
        set_core_config,
        PlatformFramework.ESP32_IDF,
        VARIANT_ESP32S3,
        cv.Version(5, 5, 2),
    )

    with pytest.raises(cv.Invalid, match="requires at least framework version 5.5.3"):
        _schema()({CONF_PSRAM_DMA: True})
