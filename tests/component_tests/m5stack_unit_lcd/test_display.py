"""Tests for the m5stack_unit_lcd display platform configuration and code generation."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
import re

import pytest

from esphome import config_validation as cv
from esphome.components.display import get_display_metadata
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.components.m5stack_unit_lcd.display import (
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    HEIGHT,
    WIDTH,
)
from esphome.config import Config
from esphome.const import (
    CONF_ADDRESS,
    CONF_BRIGHTNESS,
    CONF_FREQUENCY,
    CONF_I2C_ID,
    CONF_ID,
    CONF_INVERT_COLORS,
    CONF_LAMBDA,
    CONF_PAGES,
    CONF_ROTATION,
    CONF_UPDATE_INTERVAL,
    PlatformFramework,
)
from esphome.core import ID, Lambda, TimePeriod
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _validate(config: ConfigType, set_core_config: SetCoreConfigCallable) -> ConfigType:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    return CONFIG_SCHEMA(config)


def test_defaults(set_core_config: SetCoreConfigCallable) -> None:
    """A bare config gets the Unit LCD's fixed I2C address and sane defaults."""
    config = _validate({}, set_core_config)
    assert config[CONF_ADDRESS] == 0x3E
    assert config[CONF_BRIGHTNESS] == 1.0
    assert config[CONF_INVERT_COLORS] is False
    assert config[CONF_UPDATE_INTERVAL] == TimePeriod(seconds=1)
    assert CONF_ROTATION not in config


@pytest.mark.parametrize(
    ("value", "expected"),
    [
        pytest.param("50%", 0.5, id="percent_string"),
        pytest.param(0.25, 0.25, id="fraction"),
        pytest.param("0%", 0.0, id="zero"),
        pytest.param("100%", 1.0, id="full"),
    ],
)
def test_brightness_accepts_percentages(
    value: str | float, expected: float, set_core_config: SetCoreConfigCallable
) -> None:
    """Brightness is a percentage stored as a 0-1 fraction."""
    config = _validate({CONF_BRIGHTNESS: value}, set_core_config)
    assert config[CONF_BRIGHTNESS] == pytest.approx(expected)


@pytest.mark.parametrize(
    "value",
    [
        pytest.param("150%", id="over_100"),
        pytest.param(-0.1, id="negative"),
        pytest.param("bright", id="not_a_number"),
    ],
)
def test_brightness_rejects_invalid(
    value: str | float, set_core_config: SetCoreConfigCallable
) -> None:
    """Out-of-range or non-numeric brightness values are rejected."""
    with pytest.raises(cv.Invalid):
        _validate({CONF_BRIGHTNESS: value}, set_core_config)


def test_invert_colors_must_be_boolean(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """invert_colors only accepts booleans."""
    assert _validate({CONF_INVERT_COLORS: True}, set_core_config)[CONF_INVERT_COLORS]
    with pytest.raises(cv.Invalid):
        _validate({CONF_INVERT_COLORS: "maybe"}, set_core_config)


@pytest.mark.parametrize("rotation", [0, 90, 180, 270])
def test_rotation_accepts_right_angles(
    rotation: int, set_core_config: SetCoreConfigCallable
) -> None:
    """All four software rotations are accepted."""
    _validate({CONF_ROTATION: rotation}, set_core_config)


def test_rotation_rejects_other_angles(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Arbitrary angles are rejected; the driver only rotates in 90 degree steps."""
    with pytest.raises(cv.Invalid):
        _validate({CONF_ROTATION: 45}, set_core_config)


def test_address_can_be_overridden(set_core_config: SetCoreConfigCallable) -> None:
    """The unit's address can be changed with CHANGE_ADDR, so it must be configurable."""
    config = _validate({CONF_ADDRESS: 0x3F}, set_core_config)
    assert config[CONF_ADDRESS] == 0x3F


def test_pages_and_lambda_are_exclusive(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A display has either pages or a lambda, never both."""
    with pytest.raises(cv.Invalid, match="pages|lambda"):
        _validate(
            {
                CONF_LAMBDA: Lambda("it.draw_pixel_at(0, 0);"),
                CONF_PAGES: [{CONF_LAMBDA: Lambda("it.draw_pixel_at(0, 0);")}],
            },
            set_core_config,
        )


def test_metadata_registers_fixed_panel_size(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Validation records the panel's fixed dimensions and rotation for consumers like LVGL."""
    config = _validate(
        {CONF_ROTATION: 90, CONF_LAMBDA: Lambda("it.draw_pixel_at(0, 0);")},
        set_core_config,
    )
    meta = get_display_metadata(config[CONF_ID])
    assert meta.width == WIDTH == 135
    assert meta.height == HEIGHT == 240
    assert meta.rotation == 90
    assert meta.has_hardware_rotation is False
    assert meta.has_writer is True


def test_metadata_without_writer(set_core_config: SetCoreConfigCallable) -> None:
    """Without pages, lambda or auto clear the display reports no writer."""
    config = _validate({}, set_core_config)
    meta = get_display_metadata(config[CONF_ID])
    assert meta.has_writer is False
    assert meta.rotation == 0


def _full_config_with_bus(frequency: float) -> Config:
    """A full config carrying one I2C bus, as the ID pass leaves it for final validation."""
    bus_id = ID("i2c_bus", is_declaration=True)
    full = Config()
    full["i2c"] = [{CONF_ID: bus_id, CONF_FREQUENCY: frequency}]
    full.declare_ids.append((bus_id, ["i2c", 0, CONF_ID]))
    return full


@pytest.mark.parametrize(
    ("frequency", "valid"),
    [
        pytest.param(100_000.0, True, id="100kHz"),
        pytest.param(400_000.0, True, id="400kHz_limit"),
        pytest.param(800_000.0, False, id="800kHz_too_fast"),
    ],
)
def test_final_validate_rejects_bus_faster_than_400khz(
    frequency: float, valid: bool, set_core_config: SetCoreConfigCallable
) -> None:
    """The unit's firmware tops out at 400 kHz, so faster buses are rejected at config time."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
        full_config=_full_config_with_bus(frequency),
    )
    config = CONFIG_SCHEMA({})
    config[CONF_I2C_ID] = ID("i2c_bus", is_declaration=False)
    if valid:
        FINAL_VALIDATE_SCHEMA(config)
    else:
        with pytest.raises(cv.Invalid, match="400kHz"):
            FINAL_VALIDATE_SCHEMA(config)


def test_code_generation_wires_options(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Brightness, inversion, address, rotation, interval and the lambda reach the C++ object."""
    main_cpp = generate_main(component_config_path("basic.yaml"))

    assert "m5stack_unit_lcd::M5StackUnitLCD" in main_cpp
    # Brightness is handed to C++ as the 0-1 fraction; the driver scales it to the panel's 0-255.
    assert re.search(r"unit_lcd->set_brightness\(0\.5f\);", main_cpp)
    assert re.search(r"unit_lcd->set_invert_colors\(true\);", main_cpp)
    assert re.search(r"unit_lcd->set_i2c_address\(0x3E\);", main_cpp)
    assert re.search(
        r"unit_lcd->set_rotation\(display::DISPLAY_ROTATION_90_DEGREES\);", main_cpp
    )
    assert re.search(r"unit_lcd->set_update_interval\(2000\);", main_cpp)
    assert re.search(r"unit_lcd->set_writer\(", main_cpp)


def test_code_generation_defaults(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Defaults generate full brightness, no inversion and pages instead of a writer."""
    main_cpp = generate_main(component_config_path("defaults.yaml"))

    assert re.search(r"unit_lcd->set_brightness\(1\.0f\);", main_cpp)
    assert re.search(r"unit_lcd->set_invert_colors\(false\);", main_cpp)
    assert re.search(r"unit_lcd->set_i2c_address\(0x3E\);", main_cpp)
    assert "set_rotation(" not in main_cpp
    assert "unit_lcd->set_writer(" not in main_cpp
    assert re.search(r"unit_lcd->set_pages\(", main_cpp)


def test_light_platform_code_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The backlight light is generated and parented to the (auto-resolved) display."""
    main_cpp = generate_main(component_config_path("light.yaml"))

    output = re.search(
        r"new\((\w+)\) m5stack_unit_lcd::M5StackUnitLCDLight\(\);", main_cpp
    )
    assert output is not None, "light output not instantiated"
    assert re.search(rf"{output.group(1)}->set_parent\(unit_lcd\);", main_cpp)
    assert re.search(
        rf"new\(backlight\) light::LightState\({output.group(1)}\);", main_cpp
    )
