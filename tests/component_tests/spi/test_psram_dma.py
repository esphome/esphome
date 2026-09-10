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
    _final_validate,
    spi_device_schema,
)
from esphome.config import Config
from esphome.const import CONF_ID, CONF_SPI_ID, KEY_FRAMEWORK_VERSION, PlatformFramework
from esphome.core import CORE, ID
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
    CORE.loaded_integrations.add("psram")


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


def test_psram_dma_false_is_portable(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _stage(
        set_core_config,
        PlatformFramework.ESP32_ARDUINO,
        VARIANT_ESP32,
        cv.Version(5, 5, 2),
    )

    config = _schema()({CONF_PSRAM_DMA: False})

    assert config[CONF_PSRAM_DMA] is False


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


def test_psram_dma_requires_psram_component(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _stage(
        set_core_config,
        PlatformFramework.ESP32_IDF,
        VARIANT_ESP32S3,
        cv.Version(5, 5, 3),
    )
    CORE.loaded_integrations.remove("psram")

    with pytest.raises(cv.Invalid, match="requires component psram"):
        _schema()({CONF_PSRAM_DMA: True})


def _full_spi_config(*, hardware: bool, with_device: bool = True) -> tuple[Config, ID]:
    bus_id = ID("spi_bus", is_declaration=True, type="SPIComponent")
    bus = {CONF_ID: bus_id}
    if hardware:
        bus[CONF_INTERFACE_INDEX] = 0
    full = Config()
    full["spi"] = [bus]
    if with_device:
        full["spi_device_test"] = {
            CONF_SPI_ID: ID("spi_bus"),
            CONF_PSRAM_DMA: True,
        }
    full.declare_ids.append((bus_id, ["spi", 0, CONF_ID]))
    return full, ID("spi_bus", is_declaration=False, type="SPIComponent")


def test_psram_dma_accepts_hardware_spi(
    set_core_config: SetCoreConfigCallable,
) -> None:
    full_config, _ = _full_spi_config(hardware=True)
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config)

    _final_validate(full_config["spi"])


def test_psram_dma_rejects_software_spi(
    set_core_config: SetCoreConfigCallable,
) -> None:
    full_config, _ = _full_spi_config(hardware=False)
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config)

    with pytest.raises(cv.Invalid, match="psram_dma requires a hardware SPI") as error:
        _final_validate(full_config["spi"])

    assert error.value.path[-2:] == ["spi_device_test", CONF_PSRAM_DMA]


def test_spi_bus_rejects_psram_dma_device_without_component_final_validation(
    set_core_config: SetCoreConfigCallable,
) -> None:
    full_config, bus_id = _full_spi_config(hardware=False, with_device=False)
    full_config["device_without_final_validation"] = {
        CONF_SPI_ID: bus_id,
        CONF_PSRAM_DMA: True,
    }
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config)

    with pytest.raises(cv.Invalid, match="psram_dma requires a hardware SPI") as error:
        _final_validate(full_config["spi"])

    assert error.value.path[-2:] == [
        "device_without_final_validation",
        CONF_PSRAM_DMA,
    ]


def test_psram_dma_accepts_hardware_device_with_mixed_buses(
    set_core_config: SetCoreConfigCallable,
) -> None:
    software_bus_id = ID("software_bus", is_declaration=True, type="SPIComponent")
    hardware_bus_id = ID("hardware_bus", is_declaration=True, type="SPIComponent")
    full_config = Config()
    full_config["spi"] = [
        {CONF_ID: software_bus_id},
        {CONF_ID: hardware_bus_id, CONF_INTERFACE_INDEX: 0},
    ]
    full_config["spi_device_test"] = {
        CONF_SPI_ID: ID("hardware_bus"),
        CONF_PSRAM_DMA: True,
    }
    full_config.declare_ids.extend(
        (
            (software_bus_id, ["spi", 0, CONF_ID]),
            (hardware_bus_id, ["spi", 1, CONF_ID]),
        )
    )
    set_core_config(PlatformFramework.ESP32_IDF, full_config=full_config)

    _final_validate(full_config["spi"])
