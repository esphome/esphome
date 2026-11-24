"""Tests for the vl53l4cx sensor's address checks and code generation."""

from collections.abc import Callable
import logging
from pathlib import Path
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_TIMING_BUDGET
from esphome.components.esp32.const import KEY_COMPONENTS, KEY_REF
from esphome.components.i2c import CONF_I2C_ID
from esphome.components.vl53l4cx.sensor import (
    VL53L4CX_VERSION,
    _final_validate,
    _interrupt_capable,
    _unique_address,
    _warn_paced_by_budget,
    validate_address,
    validate_distance_mode,
    validate_timing_budget,
)
from esphome.const import (
    CONF_ADDRESS,
    CONF_ENABLE_PIN,
    CONF_ID,
    CONF_PLATFORM,
    CONF_UPDATE_INTERVAL,
    KEY_ESP32,
    SCHEDULER_DONT_RUN,
    PlatformFramework,
)
from esphome.core import CORE, ID
from tests.component_tests.types import SetCoreConfigCallable

BUS = ID("i2c_bus", is_declaration=False)


def _entry(
    address: int, enable_pin: bool = True, platform: str = "vl53l4cx"
) -> dict[str, Any]:
    entry: dict[str, Any] = {
        CONF_PLATFORM: platform,
        CONF_I2C_ID: BUS,
        CONF_ADDRESS: address,
    }
    if enable_pin:
        entry[CONF_ENABLE_PIN] = {"number": 12}
    return entry


def _bus_checks(config: dict[str, Any]) -> dict[str, Any]:
    return _unique_address(_final_validate(config))


def test_readdress_needs_enable_pin() -> None:
    with pytest.raises(cv.Invalid, match="needs an enable_pin"):
        validate_address(_entry(0x2C, enable_pin=False))
    assert validate_address(_entry(0x2C))
    assert validate_address(_entry(0x29, enable_pin=False))
    with pytest.raises(cv.Invalid, match="7-bit address"):
        validate_address(_entry(0xA9))


# ST's driver has no short mode on the VL53L4CX and caps the timing budget at 200 ms
@pytest.mark.parametrize(
    ("validator", "value"),
    [
        pytest.param(validate_distance_mode, "short", id="short mode"),
        pytest.param(validate_timing_budget, "201ms", id="budget over 200 ms"),
        pytest.param(validate_timing_budget, "9ms", id="budget under 10 ms"),
    ],
)
def test_option_limits_of_st_driver(
    validator: Callable[[str], Any], value: str
) -> None:
    with pytest.raises(cv.Invalid):
        validator(value)


@pytest.mark.parametrize(
    ("platform", "pin", "message"),
    [
        pytest.param(
            PlatformFramework.ESP32_IDF,
            {"number": 14, "allow_other_uses": True},
            "cannot be shared",
            id="shared",
        ),
        pytest.param(
            PlatformFramework.ESP8266_ARDUINO,
            {"number": 16},
            "GPIO16",
            id="ESP8266 GPIO16",
        ),
    ],
)
def test_interrupt_pin_must_raise_interrupts(
    set_core_config: SetCoreConfigCallable,
    platform: PlatformFramework,
    pin: dict[str, Any],
    message: str,
) -> None:
    set_core_config(platform)
    with pytest.raises(cv.Invalid, match=message):
        _interrupt_capable(pin)


def test_interrupt_pin_accepts_own_pin(set_core_config: SetCoreConfigCallable) -> None:
    set_core_config(PlatformFramework.ESP8266_ARDUINO)
    assert _interrupt_capable({"number": 14}) == {"number": 14}


def test_option_limits_accept_valid_values() -> None:
    assert validate_distance_mode("medium") == "medium"
    assert validate_timing_budget("200ms").total_milliseconds == 200


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
        assert _bus_checks(entry) is entry


@pytest.mark.parametrize(
    ("first", "other", "message"),
    [
        pytest.param(
            _entry(0x29),
            _entry(0x2D),
            "re-addressed away from 0x29",
            id="sibling at 0x29",
        ),
        pytest.param(
            _entry(0x2C),
            _entry(0x2C),
            "0x2C is used by more than one",
            id="same address",
        ),
        pytest.param(
            _entry(0x2C),
            _entry(0x30, platform="vl53l1x"),
            "with a VL53L1X",
            id="vl53l1x",
        ),
        pytest.param(
            _entry(0x2C),
            _entry(0x30, platform="vl53l0x"),
            "with a VL53L0X",
            id="vl53l0x",
        ),
    ],
)
def test_invalid_buses(
    set_component_config: Callable[[str, Any], None],
    first: dict[str, Any],
    other: dict[str, Any],
    message: str,
) -> None:
    set_component_config("sensor", [first, other])
    with pytest.raises(cv.Invalid, match=message):
        _bus_checks(first)


def test_vl6180x_on_the_same_bus(
    set_component_config: Callable[[str, Any], None],
) -> None:
    entry = _entry(0x29, enable_pin=False)
    set_component_config("sensor", [entry])
    set_component_config("vl6180x", [{CONF_I2C_ID: BUS, CONF_ADDRESS: 0x29}])
    with pytest.raises(cv.Invalid, match="with a VL6180X"):
        _final_validate(entry)


@pytest.mark.parametrize(
    ("interval_ms", "budget_ms", "warns"),
    [
        pytest.param(33, None, True, id="interval at the default budget"),
        pytest.param(100, 200, True, id="interval below the budget"),
        pytest.param(1000, None, False, id="default interval"),
        pytest.param(SCHEDULER_DONT_RUN, None, False, id="continuous"),
    ],
)
def test_interval_paced_by_budget_warns(
    caplog: pytest.LogCaptureFixture,
    interval_ms: int,
    budget_ms: int | None,
    warns: bool,
) -> None:
    config: dict[str, Any] = {
        CONF_ID: ID("garage", is_declaration=True),
        CONF_UPDATE_INTERVAL: cv.TimePeriod(milliseconds=interval_ms),
    }
    if budget_ms is not None:
        config[CONF_TIMING_BUDGET] = cv.TimePeriod(milliseconds=budget_ms)
    with caplog.at_level(logging.WARNING):
        _warn_paced_by_budget(config)
    assert ("update_interval: never" in caplog.text) is warns


def test_codegen(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level(logging.WARNING):
        main_cpp = generate_main(component_config_path("vl53l4cx.yaml"))
    # ratgdo's settings: farthest target past 100 mm, re-addressed, every measurement
    assert "garage->set_target(vl53l4cx::TARGET_FARTHEST);" in main_cpp
    assert "garage->set_min_distance(100);" in main_cpp
    assert "garage->set_interrupt_pin(" in main_cpp
    assert "garage->set_enable_pin(" in main_cpp
    assert "garage->set_i2c_address(0x2C);" in main_cpp
    assert "garage->set_update_interval(4294967295UL);" in main_cpp
    # Pin code is compiled in only when a sensor uses the pin
    defines = {define.name for define in CORE.defines}
    assert {"USE_VL53L4CX_ENABLE_PIN", "USE_VL53L4CX_INTERRUPT_PIN"} <= defines
    # On ESP32 the driver comes from the ESP-IDF component registry, not PlatformIO
    components = CORE.data[KEY_ESP32][KEY_COMPONENTS]
    assert components["esphome/vl53l4cx"][KEY_REF] == VL53L4CX_VERSION
    assert not any("VL53L4CX" in lib.name for lib in CORE.platformio_libraries.values())
    # Options left at their defaults emit no setter
    for setter in (
        "set_distance_mode",
        "set_timing_budget",
        "set_target",
        "set_min_distance",
        "set_offset",
    ):
        assert f"defaults->{setter}(" not in main_cpp
    # garage and the mux are on the default 50 kHz bus, defaults on a 400 kHz one. The muxed sensor sits on a channel
    # without its own frequency, which is skipped rather than traced back to the 50 kHz bus
    slow_bus = [
        r.getMessage() for r in caplog.records if "frequency: 400kHz" in r.getMessage()
    ]
    assert len(slow_bus) == 1
    assert "'garage' runs at 50 kHz" in slow_bus[0]
    assert "about 38 ms" in slow_bus[0]
    # The muxed sensor's 20 ms interval is under the 33 ms budget; the warning names its generated id
    paced = [
        r.getMessage()
        for r in caplog.records
        if "update_interval: never" in r.getMessage()
    ]
    assert len(paced) == 1
    assert "VL53L4CX 'vl53l4cx_" in paced[0]


def test_codegen_without_pins_leaves_pin_code_out(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    main_cpp = generate_main(component_config_path("vl53l4cx_no_pins.yaml"))
    assert "set_enable_pin(" not in main_cpp
    assert "set_interrupt_pin(" not in main_cpp
    defines = {define.name for define in CORE.defines}
    assert not {"USE_VL53L4CX_ENABLE_PIN", "USE_VL53L4CX_INTERRUPT_PIN"} & defines
