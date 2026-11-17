"""Tests for nRF52 hardware SPI pin selection."""

from esphome.components.mcp23xxx_base import CONF_MCP23XXX
from esphome.components.spi import (
    CONF_INTERFACE,
    CONF_INTERFACE_INDEX,
    validate_spi_config,
)
from esphome.const import (
    CONF_CLK_PIN,
    CONF_INVERTED,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    CONF_NUMBER,
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


def _spi(clk: ConfigType, mosi: ConfigType, miso: ConfigType) -> ConfigType:
    return {
        CONF_CLK_PIN: clk,
        CONF_MOSI_PIN: mosi,
        CONF_MISO_PIN: miso,
        CONF_INTERFACE: "any",
    }


def test_nrf52_internal_pins_select_hardware(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.NRF52_ZEPHYR)

    (spi,) = validate_spi_config(
        [_spi(_internal_pin(47), _internal_pin(45), _internal_pin(46))]
    )

    assert spi[CONF_INTERFACE] == "any"
    assert spi[CONF_INTERFACE_INDEX] == 0


def test_nrf52_expander_pin_falls_back_to_software(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.NRF52_ZEPHYR)

    (spi,) = validate_spi_config(
        [_spi(_internal_pin(47), _expander_pin(3), _internal_pin(46))]
    )

    assert spi[CONF_INTERFACE] == "software"
    assert CONF_INTERFACE_INDEX not in spi
