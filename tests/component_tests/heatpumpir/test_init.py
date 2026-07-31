"""Tests for the heatpumpir climate code generation."""

from collections.abc import Callable
from pathlib import Path

from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def test_defaults_visual_from_required_min_max(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    set_core_config: SetCoreConfigCallable,
) -> None:
    """With only the required min/max_temperature (no visual block), the entity
    must expose that range as its visual override, not the ClimateIR 0-100
    default (issue #17983)."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    main_cpp = generate_main(component_config_path("default_visual.yaml"))

    assert "set_visual_min_temperature_override(18.0f)" in main_cpp
    assert "set_visual_max_temperature_override(30.0f)" in main_cpp


def test_explicit_visual_takes_precedence(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
    set_core_config: SetCoreConfigCallable,
) -> None:
    """An explicit visual min/max is used as-is and not replaced by the required
    min/max_temperature."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    main_cpp = generate_main(component_config_path("explicit_visual.yaml"))

    assert "set_visual_min_temperature_override(18.0f)" in main_cpp
    assert "set_visual_max_temperature_override(30.0f)" in main_cpp
    assert "set_visual_min_temperature_override(16.0f)" not in main_cpp
    assert "set_visual_max_temperature_override(32.0f)" not in main_cpp
