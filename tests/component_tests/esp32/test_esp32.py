"""
Test ESP32 configuration
"""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome.components.esp32 import VARIANT_ESP32, VARIANTS
from esphome.components.esp32.const import KEY_ESP32, KEY_SDKCONFIG_OPTIONS, KEY_VARIANT
from esphome.components.esp32.gpio import validate_gpio_pin
import esphome.config_validation as cv
from esphome.const import (
    CONF_ESPHOME,
    CONF_IGNORE_PIN_VALIDATION_ERROR,
    CONF_NUMBER,
    PlatformFramework,
    Toolchain,
)
from esphome.core import CORE
from tests.component_tests.types import SetCoreConfigCallable


def test_esp32_config(
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF)

    from esphome.components.esp32 import CONFIG_SCHEMA, VARIANT_ESP32, VARIANT_FRIENDLY

    # Example ESP32 configuration
    config = {
        "board": "esp32dev",
        "variant": VARIANT_ESP32,
        "cpu_frequency": "240MHz",
        "flash_size": "4MB",
        "framework": {
            "type": "esp-idf",
        },
    }

    # Check if the variant is valid
    config = CONFIG_SCHEMA(config)
    assert config["variant"] == VARIANT_ESP32

    # Check that defining a variant sets the board name correctly
    for variant in VARIANTS:
        config = CONFIG_SCHEMA(
            {
                "variant": variant,
            }
        )
        assert VARIANT_FRIENDLY[variant].lower() in config["board"]


@pytest.mark.parametrize(
    ("config", "error_match"),
    [
        pytest.param(
            {"flash_size": "4MB"},
            r"This board is unknown, if you are sure you want to compile with this board selection, override with option 'variant' @ data\['board'\]",
            id="unknown_board_config",
        ),
        pytest.param(
            {"variant": "esp32xx"},
            r"Unknown value 'ESP32XX', did you mean 'ESP32', 'ESP32S3', 'ESP32S2'\? for dictionary value @ data\['variant'\]",
            id="unknown_variant_config",
        ),
        pytest.param(
            {"variant": "esp32s3", "board": "esp32dev"},
            r"Option 'variant' does not match selected board. @ data\['variant'\]",
            id="mismatched_board_variant_config",
        ),
        pytest.param(
            {
                "variant": "esp32s2",
                "framework": {
                    "type": "esp-idf",
                    "advanced": {"execute_from_psram": True},
                },
            },
            r"'execute_from_psram' is not available on this esp32 variant @ data\['framework'\]\['advanced'\]\['execute_from_psram'\]",
            id="execute_from_psram_invalid_for_variant_config",
        ),
        pytest.param(
            {
                "variant": "esp32s3",
                "framework": {
                    "type": "esp-idf",
                    "advanced": {"execute_from_psram": True},
                },
            },
            r"'execute_from_psram' requires PSRAM to be configured @ data\['framework'\]\['advanced'\]\['execute_from_psram'\]",
            id="execute_from_psram_requires_psram_s3_config",
        ),
        pytest.param(
            {
                "variant": "esp32p4",
                "framework": {
                    "type": "esp-idf",
                    "advanced": {"execute_from_psram": True},
                },
            },
            r"'execute_from_psram' requires PSRAM to be configured @ data\['framework'\]\['advanced'\]\['execute_from_psram'\]",
            id="execute_from_psram_requires_psram_p4_config",
        ),
        pytest.param(
            {
                "variant": "esp32s3",
                "framework": {
                    "type": "esp-idf",
                    "advanced": {"ignore_efuse_mac_crc": True},
                },
            },
            r"'ignore_efuse_mac_crc' is not supported on ESP32S3 @ data\['framework'\]\['advanced'\]\['ignore_efuse_mac_crc'\]",
            id="ignore_efuse_mac_crc_only_on_esp32",
        ),
    ],
)
def test_esp32_configuration_errors(
    config: Any,
    error_match: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    set_core_config(PlatformFramework.ESP32_IDF, full_config={CONF_ESPHOME: {}})
    """Test detection of invalid configuration."""
    from esphome.components.esp32 import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA

    with pytest.raises(cv.Invalid, match=error_match):
        FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA(config))


def test_execute_from_psram_s3_sdkconfig(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that execute_from_psram on ESP32-S3 sets the correct sdkconfig options."""
    generate_main(component_config_path("execute_from_psram_s3.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_SPIRAM_FETCH_INSTRUCTIONS") is True
    assert sdkconfig.get("CONFIG_SPIRAM_RODATA") is True
    assert "CONFIG_SPIRAM_XIP_FROM_PSRAM" not in sdkconfig


def test_execute_from_psram_p4_sdkconfig(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that execute_from_psram on ESP32-P4 sets the correct sdkconfig options."""
    generate_main(component_config_path("execute_from_psram_p4.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_SPIRAM_XIP_FROM_PSRAM") is True
    assert "CONFIG_SPIRAM_FETCH_INSTRUCTIONS" not in sdkconfig
    assert "CONFIG_SPIRAM_RODATA" not in sdkconfig


def test_ignore_pin_validation_error_on_clean_pin_warns(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A pin that passes validation but sets `ignore_pin_validation_error: true`
    should log a warning nudging the user to remove the flag, and not raise."""
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )

    pin = {CONF_NUMBER: 4, CONF_IGNORE_PIN_VALIDATION_ERROR: True}
    with caplog.at_level("WARNING"):
        result = validate_gpio_pin(pin)

    assert result[CONF_NUMBER] == 4
    assert "GPIO4 has no validation errors to ignore" in caplog.text


def test_ignore_pin_validation_error_on_dirty_pin_suppresses(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A pin that fails validation with `ignore_pin_validation_error: true` should
    log the suppression warning and not raise (existing behavior)."""
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )

    # GPIO6 is a flash pin on ESP32 -> pin_validation raises cv.Invalid
    pin = {CONF_NUMBER: 6, CONF_IGNORE_PIN_VALIDATION_ERROR: True}
    with caplog.at_level("WARNING"):
        result = validate_gpio_pin(pin)

    assert result[CONF_NUMBER] == 6
    assert "Ignoring validation error on pin 6" in caplog.text


def test_dirty_pin_without_ignore_flag_raises(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A pin that fails validation without the ignore flag should still raise."""
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )

    pin = {CONF_NUMBER: 6, CONF_IGNORE_PIN_VALIDATION_ERROR: False}
    with pytest.raises(cv.Invalid, match="flash interface"):
        validate_gpio_pin(pin)


def test_clean_pin_without_ignore_flag_does_not_warn(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """A clean pin without the ignore flag should pass silently."""
    set_core_config(
        PlatformFramework.ESP32_IDF, platform_data={KEY_VARIANT: VARIANT_ESP32}
    )

    pin = {CONF_NUMBER: 4, CONF_IGNORE_PIN_VALIDATION_ERROR: False}
    with caplog.at_level("WARNING"):
        result = validate_gpio_pin(pin)

    assert result[CONF_NUMBER] == 4
    assert "has no validation errors to ignore" not in caplog.text


def test_execute_from_psram_disabled_sdkconfig(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that without execute_from_psram, no XIP sdkconfig options are set."""
    generate_main(component_config_path("execute_from_psram_disabled.yaml"))
    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert "CONFIG_SPIRAM_FETCH_INSTRUCTIONS" not in sdkconfig
    assert "CONFIG_SPIRAM_RODATA" not in sdkconfig
    assert "CONFIG_SPIRAM_XIP_FROM_PSRAM" not in sdkconfig


def test_platformio_idf_enables_reproducible_build(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test PlatformIO ESP-IDF builds enable reproducible app metadata."""
    generate_main(component_config_path("reproducible_build.yaml"))

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_APP_REPRODUCIBLE_BUILD") is True


def test_platformio_arduino_enables_reproducible_build(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test PlatformIO Arduino builds enable reproducible app metadata."""
    generate_main(component_config_path("reproducible_build_arduino.yaml"))

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_APP_REPRODUCIBLE_BUILD") is True


def test_native_idf_enables_reproducible_build(
    component_config_path: Callable[[str], Path],
) -> None:
    """Test native ESP-IDF builds enable reproducible app metadata."""
    from esphome.__main__ import generate_cpp_contents
    from esphome.config import read_config

    CORE.config_path = component_config_path("reproducible_build.yaml")
    CORE.config = read_config({})
    CORE.toolchain = Toolchain.ESP_IDF
    generate_cpp_contents(CORE.config)

    sdkconfig = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    assert sdkconfig.get("CONFIG_APP_REPRODUCIBLE_BUILD") is True
