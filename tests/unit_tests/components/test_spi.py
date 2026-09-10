"""Unit tests for esphome.components.spi (Zephyr quad-mode schema gate, interface selection)."""

from __future__ import annotations

import pytest

from esphome.components.esp32 import KEY_ESP32
from esphome.components.spi import (
    CONF_CLK_PIN,
    CONF_INTERFACE,
    CONF_MISO_PIN,
    CONF_MOSI_PIN,
    _quad_platform_validator,
    _zephyr_setup_spi,
    one_of_interface_validator,
    validate_spi_config,
)
from esphome.components.zephyr.const import KEY_ZEPHYR
import esphome.config_validation as cv
from esphome.const import (
    CONF_NUMBER,
    KEY_CORE,
    KEY_TARGET_PLATFORM,
    PLATFORM_ESP32,
    PLATFORM_RP2,
    PLATFORM_ZEPHYR,
)
from esphome.core import CORE


def test_quad_platform_validator_accepts_esp32_arduino_idf() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    assert _quad_platform_validator("x") == "x"


def test_quad_platform_validator_accepts_zephyr_esp32_family_variant() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": "ESP32C6"}
    assert _quad_platform_validator("x") == "x"


def test_quad_platform_validator_rejects_zephyr_non_esp32_variant() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": "STM32F4"}
    with pytest.raises(cv.Invalid, match="Quad SPI is not available"):
        _quad_platform_validator("x")


def test_quad_platform_validator_rejects_non_esp32_non_zephyr_platform() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_RP2}
    with pytest.raises(cv.Invalid):
        _quad_platform_validator("x")


# ---------------------------------------------------------------------------
# one_of_interface_validator -- Zephyr's named-bus 'interface: spi2' selection
# (item 47: replaces the old dts_node_override key with the same shape ESP32/
# RP2040 already use). Zephyr's real bus list isn't known until to_code() runs
# fetch_board_dts(), so unlike ESP32/RP2040 a named value can't be validated
# against a real list at schema time -- it's accepted and checked for real later
# by resolve_zephyr_bus()/zephyr_setup_spi_pinctrl().
# ---------------------------------------------------------------------------


def test_one_of_interface_validator_zephyr_accepts_generic_values() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    validator = one_of_interface_validator(["software", "hardware", "any"])
    assert validator("Any") == "any"
    assert validator("software") == "software"


def test_one_of_interface_validator_zephyr_passes_through_named_bus() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    validator = one_of_interface_validator(["hardware"])
    assert validator("spi2") == "spi2"


def test_one_of_interface_validator_esp32_rejects_unknown_value() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ESP32}
    CORE.data[KEY_ESP32] = {}
    validator = one_of_interface_validator(["software", "hardware", "any"])
    with pytest.raises(cv.Invalid):
        validator("spi9")


# ---------------------------------------------------------------------------
# _zephyr_setup_spi -- item 48: multiple spi: entries are allowed on Zephyr now,
# but two entries resolving to the same real bus label is a real conflict.
# ---------------------------------------------------------------------------


def _spi_conf(interface: str, clk: int) -> dict:
    return {
        CONF_INTERFACE: interface,
        CONF_CLK_PIN: {CONF_NUMBER: clk},
    }


def test_zephyr_setup_spi_allows_distinct_buses() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {
        "board": "some_board",
        "variant": "ESP32",
        "family": "esp32",
        "prj_conf": {},
        "overlay": {"": ""},
        # resolve_zephyr_bus() -> validate_dts_label_exists() -> _get_edt() reads
        # this key directly (not .get()); DTS auto-detection then no-ops since
        # "dts_base_path" is unset here.
        "board_edt_cache": {},
    }
    resolved: set[str] = set()
    _zephyr_setup_spi(_spi_conf("spi2", 6), resolved)
    _zephyr_setup_spi(_spi_conf("spi3", 18), resolved)
    assert resolved == {"spi2", "spi3"}


def test_zephyr_setup_spi_rejects_duplicate_resolved_bus() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {
        "board": "some_board",
        "variant": "ESP32",
        "family": "esp32",
        "prj_conf": {},
        "overlay": {"": ""},
        "board_edt_cache": {},
    }
    resolved: set[str] = set()
    _zephyr_setup_spi(_spi_conf("spi2", 6), resolved)
    with pytest.raises(cv.Invalid, match="both resolved to bus 'spi2'"):
        _zephyr_setup_spi(_spi_conf("spi2", 18), resolved)


# ---------------------------------------------------------------------------
# validate_spi_config -- clk/mosi/miso checked against spi_valid_pins, same
# pattern as uart's validate_zephyr_config().
# ---------------------------------------------------------------------------


def _spi_pin_conf(clk: int, mosi: int | None = None, miso: int | None = None) -> dict:
    conf = {CONF_INTERFACE: "any", CONF_CLK_PIN: {CONF_NUMBER: clk}}
    if mosi is not None:
        conf[CONF_MOSI_PIN] = {CONF_NUMBER: mosi}
    if miso is not None:
        conf[CONF_MISO_PIN] = {CONF_NUMBER: miso}
    return conf


def test_validate_spi_config_accepts_valid_esp32_pins() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": "ESP32C6"}
    validate_spi_config([_spi_pin_conf(clk=6, mosi=7, miso=8)])


def test_validate_spi_config_rejects_invalid_esp32_clk_pin() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": "ESP32C6"}
    with pytest.raises(cv.Invalid, match="does not support SPI CLK"):
        validate_spi_config([_spi_pin_conf(clk=30)])


def test_validate_spi_config_rejects_invalid_esp32_miso_pin() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": "ESP32"}
    # GPIO34-39 are input-only on the original ESP32 -- valid for MISO, not for CLK.
    with pytest.raises(cv.Invalid, match="does not support SPI MISO"):
        validate_spi_config([_spi_pin_conf(clk=6, miso=99)])


def test_validate_spi_config_accepts_valid_nordic_pins() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": "NRF52"}
    validate_spi_config([_spi_pin_conf(clk=8, mosi=6, miso=7)])


def test_validate_spi_config_rejects_invalid_nordic_clk_pin() -> None:
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": "NRF52"}
    with pytest.raises(cv.Invalid, match="does not support SPI CLK"):
        validate_spi_config([_spi_pin_conf(clk=99)])


def test_validate_spi_config_skips_pin_check_for_non_esp32_family() -> None:
    # STM32's spi_valid_pins is empty -- pin validity is left to
    # zephyr_setup_spi_pinctrl() at codegen time, not checked here.
    CORE.data[KEY_CORE] = {KEY_TARGET_PLATFORM: PLATFORM_ZEPHYR}
    CORE.data[KEY_ZEPHYR] = {"variant": "STM32F4"}
    validate_spi_config([_spi_pin_conf(clk=99, mosi=99, miso=99)])
