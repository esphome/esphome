"""Tests for the esp32_hosted ESP-IDF version gate and esp_hosted line selection."""

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_IDF_VERSION
from esphome.components.esp32_hosted import (
    CONF_ACTIVE_HIGH,
    CONF_BUS_WIDTH,
    _final_validate,
    uses_esp_hosted_3x,
)
from esphome.const import CONF_TYPE, PlatformFramework

from ..types import SetCoreConfigCallable


@pytest.mark.parametrize("idf", ["5.3.0", "5.4.2", "5.5.5"])
def test_final_validate_accepts_supported_idf(
    set_core_config: SetCoreConfigCallable, idf: str
) -> None:
    """ESP-IDF 5.3 and newer passes validation unchanged."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
    )
    _final_validate({})


@pytest.mark.parametrize("idf", ["5.0.0", "5.2.2"])
def test_final_validate_rejects_old_idf(
    set_core_config: SetCoreConfigCallable, idf: str
) -> None:
    """ESP-IDF older than 5.3 is rejected with a clear error."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
    )
    with pytest.raises(cv.Invalid, match="requires ESP-IDF 5.3 or newer"):
        _final_validate({})


SDIO_4BIT = {CONF_TYPE: "sdio", CONF_BUS_WIDTH: 4, CONF_ACTIVE_HIGH: True}
SDIO_1BIT = {CONF_TYPE: "sdio", CONF_BUS_WIDTH: 1, CONF_ACTIVE_HIGH: True}
SPI = {CONF_TYPE: "spi", CONF_ACTIVE_HIGH: True}
SPI_ACTIVE_LOW = {CONF_TYPE: "spi", CONF_ACTIVE_HIGH: False}


@pytest.mark.parametrize(
    ("idf", "config", "expected"),
    [
        ("5.3.0", SDIO_4BIT, False),
        ("5.4.2", SPI, False),
        ("5.5.0", SDIO_4BIT, True),
        ("5.5.5", SPI, True),
        ("5.5.5", SDIO_1BIT, False),
        ("5.5.5", SPI_ACTIVE_LOW, False),
    ],
)
def test_uses_esp_hosted_3x(
    set_core_config: SetCoreConfigCallable,
    idf: str,
    config: dict,
    expected: bool,
) -> None:
    """3.x needs ESP-IDF 5.5, a 4-bit SDIO or SPI bus, and an active-high reset."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
    )
    assert uses_esp_hosted_3x(config) is expected
