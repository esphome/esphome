"""Tests for the vl53l4cx sensor's address checks and code generation."""

from collections.abc import Callable, Generator
from pathlib import Path
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.vl53l4cx.sensor import _final_validate, validate_address
from esphome.config import Config
from esphome.const import CONF_ADDRESS, CONF_ENABLE_PIN, CONF_PLATFORM
from esphome.core import CORE, ID
import esphome.final_validate as fv

BUS = ID("i2c_bus", is_declaration=False)


def _entry(address: int, enable_pin: bool = True) -> dict[str, Any]:
    entry: dict[str, Any] = {
        CONF_PLATFORM: "vl53l4cx",
        "i2c_id": BUS,
        CONF_ADDRESS: address,
    }
    if enable_pin:
        entry[CONF_ENABLE_PIN] = {"number": 12}
    return entry


@pytest.fixture
def sensors() -> Generator[Callable[..., list[dict[str, Any]]]]:
    """Install the given sensor entries as the full config final validation sees."""
    tokens = []

    def install(*entries: dict[str, Any]) -> list[dict[str, Any]]:
        full = Config()
        full["sensor"] = list(entries)
        tokens.append(fv.full_config.set(full))
        return list(entries)

    yield install
    for token in reversed(tokens):
        fv.full_config.reset(token)


def test_readdress_needs_enable_pin() -> None:
    with pytest.raises(cv.Invalid, match="needs an enable_pin"):
        validate_address(_entry(0x2C, enable_pin=False))
    assert validate_address(_entry(0x2C))
    assert validate_address(_entry(0x29, enable_pin=False))


def test_single_sensor_needs_no_enable_pin(sensors: Callable[..., list]) -> None:
    (entry,) = sensors(_entry(0x29, enable_pin=False))
    assert _final_validate(entry) is entry


@pytest.mark.parametrize(
    ("first", "second", "message"),
    [
        (_entry(0x2C, enable_pin=False), _entry(0x2D), "needs an enable_pin"),
        (_entry(0x29), _entry(0x2D), "re-addressed away from 0x29"),
        (_entry(0x2C), _entry(0x2C), "used by more than one"),
    ],
)
def test_shared_bus_rules(
    sensors: Callable[..., list], first: dict, second: dict, message: str
) -> None:
    entries = sensors(first, second)
    with pytest.raises(cv.Invalid, match=message):
        _final_validate(entries[0])


def test_shared_bus_with_own_pins_and_addresses(sensors: Callable[..., list]) -> None:
    for entry in sensors(_entry(0x2C), _entry(0x2D)):
        assert _final_validate(entry) is entry


def test_ratgdo_style_config_generates_driver(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("vl53l4cx.yaml"))
    assert "vl53l4cx::VL53L4CXSensor" in main_cpp
    assert "garage->set_target(vl53l4cx::TARGET_FARTHEST);" in main_cpp
    assert "garage->set_min_distance(100);" in main_cpp
    assert "garage->set_distance_mode(3);" in main_cpp
    assert "garage->set_timing_budget(33);" in main_cpp
    assert "garage->set_interrupt_pin(" in main_cpp
    assert "garage->set_enable_pin(" in main_cpp
    assert "garage->set_i2c_address(0x2C);" in main_cpp
    assert any(
        lib.name == "esphome/VL53L4CX" for lib in CORE.platformio_libraries.values()
    )
