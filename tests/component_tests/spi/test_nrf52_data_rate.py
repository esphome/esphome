"""Tests for nRF52 SPI data rate validation."""

import pytest

from esphome import config_validation as cv
from esphome.components.mcp23xxx_base import CONF_MCP23XXX
from esphome.components.spi import (
    CONF_INTERFACE,
    SPI_DATA_RATE_SCHEMA,
    _final_validate,
    validate_spi_config,
)
from esphome.const import (
    CONF_CLK_PIN,
    CONF_DATA_RATE,
    CONF_ID,
    CONF_INVERTED,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_NUMBER,
    CONF_SPI_ID,
    PlatformFramework,
)
from esphome.core import ID
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _internal_pin(number: int) -> ConfigType:
    return {CONF_NUMBER: number, CONF_INVERTED: False}


def _expander_pin(number: int) -> ConfigType:
    return {
        CONF_MCP23XXX: ID("mcp23017_hub", is_declaration=False),
        CONF_NUMBER: number,
        CONF_INVERTED: False,
    }


def _validate(
    set_core_config: SetCoreConfigCallable,
    mosi: ConfigType,
    interface: str,
    data_rate: str,
) -> None:
    """Resolve one bus, then final validate a device on it at data_rate."""
    set_core_config(PlatformFramework.NRF52_ZEPHYR)
    (bus,) = validate_spi_config(
        [
            {
                CONF_ID: ID("spi_bus", is_declaration=True, type="SPIComponent"),
                CONF_CLK_PIN: _internal_pin(47),
                CONF_MOSI_PIN: mosi,
                CONF_MISO_PIN: _internal_pin(46),
                CONF_INTERFACE: interface,
            }
        ]
    )
    set_core_config(
        PlatformFramework.NRF52_ZEPHYR,
        full_config={
            "spi": [bus],
            "spi_device_test": {
                CONF_SPI_ID: ID("spi_bus"),
                CONF_DATA_RATE: SPI_DATA_RATE_SCHEMA(data_rate),
            },
        },
    )
    _final_validate([bus])


@pytest.mark.parametrize("data_rate", ["125kHz", "130kHz", "1MHz", "8MHz"])
def test_nrf52_hardware_bus_accepts_available_rate(
    set_core_config: SetCoreConfigCallable, data_rate: str
) -> None:
    _validate(set_core_config, _internal_pin(45), "any", data_rate)


def test_nrf52_hardware_bus_rejects_rate_below_lowest_step(
    set_core_config: SetCoreConfigCallable,
) -> None:
    with pytest.raises(cv.Invalid, match="below 125kHz") as error:
        _validate(set_core_config, _internal_pin(45), "any", "120kHz")

    assert error.value.path[-2:] == ["spi_device_test", CONF_DATA_RATE]


def test_nrf52_hardware_bus_rejects_rate_between_steps(
    set_core_config: SetCoreConfigCallable,
) -> None:
    with pytest.raises(cv.Invalid, match="closest is 250kHz") as error:
        _validate(set_core_config, _internal_pin(45), "any", "300kHz")

    assert error.value.path[-2:] == ["spi_device_test", CONF_DATA_RATE]


def test_nrf52_software_bus_accepts_any_rate(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _validate(set_core_config, _internal_pin(45), "software", "100kHz")


def test_nrf52_expander_fallback_bus_accepts_any_rate(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _validate(set_core_config, _expander_pin(3), "any", "100kHz")
