"""Tests for the GPIO 1-wire RMT transport selection."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

import esphome.config_validation as cv
from esphome.core import CORE

HERE = Path(__file__).parent
RMT_DEFINE = "USE_ONE_WIRE_RMT"


def _rmt_driver_excluded() -> bool:
    from esphome.components.esp32 import KEY_ESP32, KEY_EXCLUDE_COMPONENTS

    return "esp_driver_rmt" in CORE.data.get(KEY_ESP32, {}).get(
        KEY_EXCLUDE_COMPONENTS, set()
    )


def test_default_uses_gpio_per_instance(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32_idf.yaml")

    assert "ow_bus->set_use_rmt(false);" in main_cpp
    assert RMT_DEFINE not in {define.name for define in CORE.defines}
    assert _rmt_driver_excluded()


def test_explicit_gpio_uses_gpio_per_instance(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32_idf_use_rmt_false.yaml")

    assert "ow_bus->set_use_rmt(false);" in main_cpp
    assert RMT_DEFINE not in {define.name for define in CORE.defines}
    assert _rmt_driver_excluded()


def test_rmt_bus_enables_rmt_support(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32_idf_use_rmt_true.yaml")

    assert "ow_bus->set_use_rmt(true);" in main_cpp
    assert RMT_DEFINE in {define.name for define in CORE.defines}
    assert not _rmt_driver_excluded()


def test_mixed_buses_select_transport_independently(
    generate_main: Callable[[str | Path], str],
) -> None:
    """One RMT bus must not force another GPIO bus to use RMT."""
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32_mixed_buses.yaml")

    assert "ow_rmt->set_use_rmt(true);" in main_cpp
    assert "ow_gpio->set_use_rmt(false);" in main_cpp
    assert "App.register_component_(ow_rmt" in main_cpp
    assert "App.register_component_(ow_gpio" in main_cpp
    assert RMT_DEFINE in {define.name for define in CORE.defines}
    assert not _rmt_driver_excluded()


def test_rmt_source_is_kept_when_enabled(
    generate_main: Callable[[str | Path], str],
) -> None:
    generate_main(HERE / "test_gpio_one_wire_esp32_idf_use_rmt_true.yaml")

    from esphome.components.gpio.one_wire import FILTER_SOURCE_FILES

    excluded = FILTER_SOURCE_FILES()
    assert "gpio_one_wire_rmt.cpp" not in excluded
    assert "gpio_one_wire.cpp" not in excluded


def test_rmt_source_is_excluded_when_unused(
    generate_main: Callable[[str | Path], str],
) -> None:
    generate_main(HERE / "test_gpio_one_wire_esp32_idf.yaml")

    from esphome.components.gpio.one_wire import FILTER_SOURCE_FILES

    excluded = FILTER_SOURCE_FILES()
    assert "gpio_one_wire_rmt.cpp" in excluded
    assert "gpio_one_wire.cpp" not in excluded


def test_use_rmt_on_esp8266_is_rejected(
    generate_main: Callable[[str | Path], str],
) -> None:
    with pytest.raises(cv.Invalid, match="ESP32"):
        generate_main(HERE / "test_gpio_one_wire_esp8266_use_rmt_true.yaml")


def test_use_rmt_on_esp32_without_rmt_is_rejected(
    generate_main: Callable[[str | Path], str],
) -> None:
    with pytest.raises(cv.Invalid, match="use_rmt.*not available.*ESP32C2|no RMT hardware"):
        generate_main(HERE / "test_gpio_one_wire_esp32c2_use_rmt_true.yaml")


def test_default_gpio_on_esp32_without_rmt_is_allowed(
    generate_main: Callable[[str | Path], str],
) -> None:
    main_cpp = generate_main(HERE / "test_gpio_one_wire_esp32c2_default.yaml")

    assert "ow_bus->set_use_rmt(false);" in main_cpp
    assert RMT_DEFINE not in {define.name for define in CORE.defines}
    assert _rmt_driver_excluded()
