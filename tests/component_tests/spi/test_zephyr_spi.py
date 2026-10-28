"""Tests for SPI on platform: zephyr (quad-mode gate, interface selection, pin checks)."""

import pytest

from esphome.components.esp32 import VARIANT_ESP32
from esphome.components.spi import (
    CONF_CLK_PIN,
    CONF_INTERFACE,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    _quad_platform_validator,
    _validate_clk_pin_required,
    _zephyr_setup_spi,
    one_of_interface_validator,
    validate_spi_config,
)
import esphome.config_validation as cv
from esphome.const import CONF_NUMBER, KEY_VARIANT, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable
from tests.unit_tests.components.zephyr.zephyr_state import empty_zephyr_data


def _zephyr(set_core_config: SetCoreConfigCallable, variant: str | None = None) -> None:
    set_core_config(
        PlatformFramework.ZEPHYR_ZEPHYR,
        platform_data=empty_zephyr_data(variant=variant),
    )


def test_quad_platform_validator_accepts_esp32_arduino_idf(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    assert _quad_platform_validator("x") == "x"


def test_quad_platform_validator_accepts_zephyr_esp32_family_variant(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "ESP32C6")
    assert _quad_platform_validator("x") == "x"


def test_quad_platform_validator_rejects_zephyr_non_esp32_variant(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "STM32F4")
    with pytest.raises(cv.Invalid, match="Quad SPI is not available"):
        _quad_platform_validator("x")


def test_quad_platform_validator_rejects_non_esp32_non_zephyr_platform(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.RP2_ARDUINO)
    with pytest.raises(cv.Invalid):
        _quad_platform_validator("x")


# Zephyr's bus list is only known from the board DTS in to_code(), so a named
# interface is accepted here and checked later by resolve_zephyr_bus().


def test_one_of_interface_validator_zephyr_accepts_generic_values(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config)
    validator = one_of_interface_validator(["software", "hardware", "any"])
    assert validator("Any") == "any"
    assert validator("software") == "software"


def test_one_of_interface_validator_zephyr_passes_through_named_bus(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config)
    validator = one_of_interface_validator(["hardware"])
    assert validator("spi2") == "spi2"


def test_one_of_interface_validator_esp32_rejects_unknown_value(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )
    validator = one_of_interface_validator(["software", "hardware", "any"])
    with pytest.raises(cv.Invalid):
        validator("spi9")


def _spi_conf(interface: str, clk: int) -> dict:
    return {
        CONF_INTERFACE: interface,
        CONF_CLK_PIN: {CONF_NUMBER: clk},
    }


def test_zephyr_setup_spi_allows_distinct_buses(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "ESP32")
    resolved: set[str] = set()
    _zephyr_setup_spi(_spi_conf("spi2", 6), resolved)
    _zephyr_setup_spi(_spi_conf("spi3", 18), resolved)
    assert resolved == {"spi2", "spi3"}


def test_zephyr_setup_spi_rejects_duplicate_resolved_bus(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "ESP32")
    resolved: set[str] = set()
    _zephyr_setup_spi(_spi_conf("spi2", 6), resolved)
    with pytest.raises(cv.Invalid, match="both resolved to bus 'spi2'"):
        _zephyr_setup_spi(_spi_conf("spi2", 18), resolved)


def _spi_pin_conf(clk: int, mosi: int | None = None, miso: int | None = None) -> dict:
    conf = {CONF_INTERFACE: "any", CONF_CLK_PIN: {CONF_NUMBER: clk}}
    if mosi is not None:
        conf[CONF_MOSI_PIN] = {CONF_NUMBER: mosi}
    if miso is not None:
        conf[CONF_MISO_PIN] = {CONF_NUMBER: miso}
    return conf


def test_validate_spi_config_accepts_valid_esp32_pins(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "ESP32C6")
    validate_spi_config([_spi_pin_conf(clk=6, mosi=7, miso=8)])


def test_validate_spi_config_rejects_invalid_esp32_clk_pin(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "ESP32C6")
    with pytest.raises(cv.Invalid, match="does not support SPI CLK"):
        validate_spi_config([_spi_pin_conf(clk=30)])


def test_validate_spi_config_rejects_invalid_esp32_miso_pin(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "ESP32")
    with pytest.raises(cv.Invalid, match="does not support SPI MISO"):
        validate_spi_config([_spi_pin_conf(clk=18, miso=99)])


def test_validate_spi_config_rejects_invalid_esp32_mosi_pin(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "ESP32")
    with pytest.raises(cv.Invalid, match="does not support SPI MOSI"):
        validate_spi_config([_spi_pin_conf(clk=18, mosi=99)])


def test_validate_spi_config_accepts_input_only_pin_for_miso(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # GPIO34-39 are input-only on the original ESP32: fine for MISO.
    _zephyr(set_core_config, "ESP32")
    validate_spi_config([_spi_pin_conf(clk=18, mosi=23, miso=34)])


@pytest.mark.parametrize(
    ("signal", "conf"), [("CLK", {"clk": 34}), ("MOSI", {"clk": 18, "mosi": 34})]
)
def test_validate_spi_config_rejects_input_only_pin_for_output_signals(
    set_core_config: SetCoreConfigCallable, signal: str, conf: dict[str, int]
) -> None:
    _zephyr(set_core_config, "ESP32")
    with pytest.raises(cv.Invalid, match=f"does not support SPI {signal}"):
        validate_spi_config([_spi_pin_conf(**conf)])


@pytest.mark.parametrize(
    ("variant", "pin"), [("ESP32", 6), ("ESP32C3", 12), ("ESP32C5", 16)]
)
def test_validate_spi_config_rejects_esp32_flash_pins(
    set_core_config: SetCoreConfigCallable, variant: str, pin: int
) -> None:
    _zephyr(set_core_config, variant)
    with pytest.raises(cv.Invalid, match="does not support SPI CLK"):
        validate_spi_config([_spi_pin_conf(clk=pin)])


def test_validate_spi_config_accepts_valid_nordic_pins(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "NRF52")
    validate_spi_config([_spi_pin_conf(clk=8, mosi=6, miso=7)])


def test_validate_spi_config_rejects_invalid_nordic_clk_pin(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "NRF52")
    with pytest.raises(cv.Invalid, match="does not support SPI CLK"):
        validate_spi_config([_spi_pin_conf(clk=99)])


def test_validate_spi_config_accepts_valid_silabs_pins(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "EFR32MG24")
    validate_spi_config([_spi_pin_conf(clk=3, mosi=5, miso=6)])


def test_validate_spi_config_rejects_invalid_silabs_clk_pin(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "EFR32MG24")
    with pytest.raises(cv.Invalid, match="does not support SPI CLK"):
        validate_spi_config([_spi_pin_conf(clk=99)])


def test_validate_spi_config_rejects_pin_missing_from_xg24(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config, "EFR32MG24")
    with pytest.raises(cv.Invalid, match="does not support SPI CLK"):
        validate_spi_config([_spi_pin_conf(clk=10)])  # PA10


def test_validate_spi_config_skips_pin_check_for_non_esp32_family(
    set_core_config: SetCoreConfigCallable,
) -> None:
    # STM32 has no spi_valid_pins; zephyr_setup_spi_pinctrl() checks pins at codegen
    _zephyr(set_core_config, "STM32F4")
    validate_spi_config([_spi_pin_conf(clk=99, mosi=99, miso=99)])


def test_clk_pin_required_zephyr_hardware_allows_missing_clk(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config)
    value = {CONF_INTERFACE: "any", CONF_MOSI_PIN: {CONF_NUMBER: 7}}
    assert _validate_clk_pin_required(value) is value


def test_clk_pin_required_zephyr_software_requires_clk(
    set_core_config: SetCoreConfigCallable,
) -> None:
    _zephyr(set_core_config)
    value = {CONF_INTERFACE: "software", CONF_MOSI_PIN: {CONF_NUMBER: 7}}
    with pytest.raises(cv.Invalid):
        _validate_clk_pin_required(value)


def test_clk_pin_required_non_zephyr_requires_clk(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)
    value = {CONF_INTERFACE: "any"}
    with pytest.raises(cv.Invalid):
        _validate_clk_pin_required(value)
