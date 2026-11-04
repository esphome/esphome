"""Tests for the esp32_hosted validation."""

import logging

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_IDF_VERSION,
    KEY_VARIANT,
    VARIANT_ESP32P4,
)
from esphome.components.esp32_hosted import (
    CONF_ACTIVE_HIGH,
    CONF_BUS_WIDTH,
    CONF_CMD_PIN,
    CONF_D0_PIN,
    CONF_D1_PIN,
    CONF_D2_PIN,
    CONF_D3_PIN,
    CONFIG_SCHEMA,
    _final_validate,
    _validate_active_high,
)
from esphome.const import CONF_CLK_PIN, CONF_RESET_PIN, CONF_VARIANT, PlatformFramework
from esphome.types import ConfigType

from ..types import SetCoreConfigCallable

SDIO_PINS: ConfigType = {
    CONF_VARIANT: "ESP32C6",
    CONF_RESET_PIN: 54,
    CONF_CMD_PIN: 19,
    CONF_CLK_PIN: 18,
    CONF_D0_PIN: 14,
    CONF_D1_PIN: 15,
    CONF_D2_PIN: 16,
    CONF_D3_PIN: 17,
}


@pytest.fixture
def p4_core(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={
            KEY_BOARD: "esp32-p4-evboard",
            KEY_IDF_VERSION: cv.Version.parse("5.5.6"),
            KEY_VARIANT: VARIANT_ESP32P4,
        },
    )


@pytest.mark.parametrize("idf", ["5.0.0", "5.2.2", "5.3.2", "5.4.2"])
def test_final_validate_rejects_old_idf(
    set_core_config: SetCoreConfigCallable, idf: str
) -> None:
    """ESP-IDF older than 5.5 is rejected with a clear error."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
    )
    with pytest.raises(cv.Invalid, match="requires ESP-IDF 5.5 or newer"):
        _final_validate({})


@pytest.mark.parametrize("idf", ["5.5.0", "5.5.6"])
def test_final_validate_accepts_supported_idf(
    set_core_config: SetCoreConfigCallable, idf: str
) -> None:
    """ESP-IDF 5.5 and newer passes validation unchanged."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_IDF_VERSION: cv.Version.parse(idf)},
    )
    _final_validate({})


@pytest.mark.usefixtures("p4_core")
@pytest.mark.parametrize("bus_width", [1, 4])
def test_sdio_requires_d1_pin(bus_width: int) -> None:
    """D1 carries the card interrupt, so a 1-bit bus needs it too."""
    config = {k: v for k, v in SDIO_PINS.items() if k != CONF_D1_PIN}
    config[CONF_BUS_WIDTH] = bus_width
    with pytest.raises(cv.Invalid, match="required key not provided"):
        CONFIG_SCHEMA(config)


@pytest.mark.usefixtures("p4_core")
@pytest.mark.parametrize("pin", [CONF_D2_PIN, CONF_D3_PIN])
def test_sdio_4bit_requires_d2_d3(pin: str) -> None:
    """A 4-bit bus needs D2 and D3."""
    config = {k: v for k, v in SDIO_PINS.items() if k != pin}
    with pytest.raises(cv.Invalid, match=f"{pin} is required when bus_width is 4"):
        CONFIG_SCHEMA(config)


@pytest.mark.usefixtures("p4_core")
def test_sdio_1bit_accepts_d0_d1_only() -> None:
    """A 1-bit bus validates with just D0 and D1."""
    config = {k: v for k, v in SDIO_PINS.items() if k not in (CONF_D2_PIN, CONF_D3_PIN)}
    config[CONF_BUS_WIDTH] = 1
    CONFIG_SCHEMA(config)


def test_active_high_absent_is_silent(caplog: pytest.LogCaptureFixture) -> None:
    """A config without the deprecated key logs nothing."""
    with caplog.at_level(logging.WARNING):
        assert _validate_active_high({}) == {}
    assert "active_high" not in caplog.text


def test_active_high_true_warns(caplog: pytest.LogCaptureFixture) -> None:
    """The old required value still validates, with a deprecation warning."""
    with caplog.at_level(logging.WARNING):
        _validate_active_high({CONF_ACTIVE_HIGH: True})
    assert "'active_high' is deprecated" in caplog.text


def test_active_high_false_rejected() -> None:
    """esp_hosted drives the reset line with a low pulse only."""
    with pytest.raises(cv.Invalid, match="active_high: false"):
        _validate_active_high({CONF_ACTIVE_HIGH: False})
