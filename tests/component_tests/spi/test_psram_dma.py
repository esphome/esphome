"""Tests for SPI PSRAM DMA configuration validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
)
from esphome.components.spi import (
    CONF_INTERFACE_INDEX,
    CONF_PSRAM_DMA,
    final_validate_device_schema,
    spi_device_schema,
)
from esphome.config import Config
from esphome.const import CONF_ID, CONF_SPI_ID, KEY_FRAMEWORK_VERSION, PlatformFramework
from esphome.core import ID
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


def test_psram_dma_accepts_esp32s31(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _stage(
        set_core_config,
        PlatformFramework.ESP32_IDF,
        VARIANT_ESP32S31,
        cv.Version(6, 0, 0),
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


def _full_spi_config(*, hardware: bool) -> tuple[Config, ID]:
    bus_id = ID("spi_bus", is_declaration=True, type="SPIComponent")
    bus = {CONF_ID: bus_id}
    if hardware:
        bus[CONF_INTERFACE_INDEX] = 0
    full = Config()
    full["spi"] = [bus]
    full.declare_ids.append((bus_id, ["spi", 0, CONF_ID]))
    return full, ID("spi_bus", is_declaration=False, type="SPIComponent")


def test_psram_dma_accepts_hardware_spi(
    set_core_config: SetCoreConfigCallable,
) -> None:
    full_config, bus_id = _full_spi_config(hardware=True)
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config)

    validator = final_validate_device_schema(
        "test", require_mosi=False, require_miso=False
    )
    validator({CONF_SPI_ID: bus_id, CONF_PSRAM_DMA: True})


def test_psram_dma_rejects_software_spi(
    set_core_config: SetCoreConfigCallable,
) -> None:
    full_config, bus_id = _full_spi_config(hardware=False)
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config)

    validator = final_validate_device_schema(
        "test", require_mosi=False, require_miso=False
    )
    with pytest.raises(cv.Invalid, match="psram_dma requires a hardware SPI"):
        validator({CONF_SPI_ID: bus_id, CONF_PSRAM_DMA: True})
