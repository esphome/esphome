"""Tests for the vl53l4cx sensor's address checks and code generation."""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.vl53l4cx.sensor import _final_validate, validate_address
from esphome.const import CONF_ADDRESS, CONF_ENABLE_PIN, CONF_PLATFORM
from esphome.core import CORE, ID

BUS = ID("i2c_bus", is_declaration=False)


def _entry(
    address: int, enable_pin: bool = True, platform: str = "vl53l4cx"
) -> dict[str, Any]:
    entry: dict[str, Any] = {
        CONF_PLATFORM: platform,
        "i2c_id": BUS,
        CONF_ADDRESS: address,
    }
    if enable_pin:
        entry[CONF_ENABLE_PIN] = {"number": 12}
    return entry


def test_readdress_needs_enable_pin() -> None:
    with pytest.raises(cv.Invalid, match="needs an enable_pin"):
        validate_address(_entry(0x2C, enable_pin=False))
    assert validate_address(_entry(0x2C))
    assert validate_address(_entry(0x29, enable_pin=False))


@pytest.mark.parametrize(
    "entries",
    [
        pytest.param([_entry(0x29, enable_pin=False)], id="single sensor at 0x29"),
        pytest.param([_entry(0x2C), _entry(0x2D)], id="own pins and addresses"),
    ],
)
def test_valid_buses(
    set_component_config: Callable[[str, Any], None], entries: list[dict[str, Any]]
) -> None:
    set_component_config("sensor", entries)
    for entry in entries:
        assert _final_validate(entry) is entry


@pytest.mark.parametrize(
    ("other", "message"),
    [
        pytest.param(_entry(0x2D), "re-addressed away from 0x29", id="sibling at 0x29"),
        pytest.param(_entry(0x2C), "used by more than one", id="same address"),
        pytest.param(
            _entry(0x30, platform="vl53l1x"), "cannot share an I2C bus", id="vl53l1x"
        ),
        pytest.param(
            _entry(0x30, platform="vl53l0x"), "cannot share an I2C bus", id="vl53l0x"
        ),
    ],
)
def test_invalid_buses(
    set_component_config: Callable[[str, Any], None], other: dict, message: str
) -> None:
    first = _entry(0x29) if "0x29" in message else _entry(0x2C)
    set_component_config("sensor", [first, other])
    with pytest.raises(cv.Invalid, match=message):
        _final_validate(first)


def test_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("vl53l4cx.yaml"))
    # ratgdo's settings: farthest target past 100 mm, re-addressed, every measurement
    assert "garage->set_target(vl53l4cx::TARGET_FARTHEST);" in main_cpp
    assert "garage->set_min_distance(100);" in main_cpp
    assert "garage->set_interrupt_pin(" in main_cpp
    assert "garage->set_enable_pin(" in main_cpp
    assert "garage->set_i2c_address(0x2C);" in main_cpp
    assert "garage->set_update_interval(4294967295UL);" in main_cpp
    assert any(
        lib.name == "esphome/VL53L4CX" for lib in CORE.platformio_libraries.values()
    )
    # Options left at their defaults emit no setter
    for setter in (
        "set_distance_mode",
        "set_timing_budget",
        "set_target",
        "set_min_distance",
        "set_offset",
    ):
        assert f"defaults->{setter}(" not in main_cpp
