"""Tests for ATM90E32 threshold validation and code generation."""

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_BOARD, VARIANT_ESP32
from esphome.const import KEY_VARIANT, PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def _base_config(current_peak: str) -> dict:
    return {
        "cs_pin": 16,
        "line_frequency": "60Hz",
        "phase_a": {
            "current": {
                "name": "Phase A Current",
            }
        },
        "thresholds": {
            "current_peak": current_peak,
        },
    }


def test_frequency_threshold_validation_success(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Allow explicit frequency thresholds without a separate nominal frequency."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    from esphome.components.atm90e32 import sensor as atm90e32_sensor

    config = _base_config("65.535A")
    config["thresholds"]["frequency_low_hz"] = "58Hz"
    config["thresholds"]["frequency_high_hz"] = "62Hz"

    validated = atm90e32_sensor.CONFIG_SCHEMA(config)
    assert validated["thresholds"]["frequency_low_hz"] == pytest.approx(58.0)
    assert validated["thresholds"]["frequency_high_hz"] == pytest.approx(62.0)


def test_current_peak_threshold_validation_success(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Allow current_peak at the native ATM90E32 register limit."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    from esphome.components.atm90e32 import sensor as atm90e32_sensor

    config = atm90e32_sensor.CONFIG_SCHEMA(_base_config("65.535A"))
    assert config["thresholds"]["current_peak"] == pytest.approx(65.535)


def test_current_peak_threshold_validation_error(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Reject current_peak values above the native ATM90E32 register limit."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    from esphome.components.atm90e32 import sensor as atm90e32_sensor

    with pytest.raises(
        cv.Invalid, match=r"thresholds\.current_peak must be 65\.535A or less"
    ):
        atm90e32_sensor.CONFIG_SCHEMA(_base_config("65.536A"))


def test_code_generation(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
) -> None:
    """Generate code for a valid ATM90E32 thresholds configuration."""
    main_cpp = generate_main(component_fixture_path("atm90e32.yaml"))

    assert "set_threshold_current_peak_a(65.535f);" in main_cpp
    assert "set_threshold_frequency_low_hz(58.0f);" in main_cpp
    assert "set_threshold_frequency_high_hz(62.0f);" in main_cpp
    assert "set_threshold_frequency_nominal_hz" not in main_cpp
