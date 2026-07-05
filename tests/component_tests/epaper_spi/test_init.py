"""Tests for epaper_spi configuration validation."""

from collections.abc import Callable
from pathlib import Path
import re
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.epaper_spi.display import (
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    MODELS,
)
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_CS_PIN,
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_ENABLE_PIN,
    CONF_HEIGHT,
    CONF_INIT_SEQUENCE,
    CONF_RESET_PIN,
    CONF_WIDTH,
    PlatformFramework,
)
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable

# Pin options whose values must be valid on the chosen ESP32 variant.
_PIN_CONF_KEYS = (
    CONF_CS_PIN,
    CONF_DC_PIN,
    CONF_RESET_PIN,
    CONF_BUSY_PIN,
    CONF_ENABLE_PIN,
)


def _pins_for(model: Any, config: ConfigType) -> list:
    """Collect every GPIO the config will actually use (model defaults or injected)."""
    pins: list = []
    for key in _PIN_CONF_KEYS:
        # An injected value in the config takes precedence over the model default.
        value = config[key] if key in config else model.get_default(key)
        if not value:  # get_default returns False for pins the model omits
            continue
        if isinstance(value, list):
            pins.extend(value)
        else:
            pins.append(value)
    return pins


def run_schema_validation(
    config: ConfigType, with_final_validate: bool = False
) -> None:
    """Run schema validation on a configuration.

    Args:
        config: The configuration to validate
        with_final_validate: If True, also run final validation (requires full config setup)
    """
    result = CONFIG_SCHEMA(config)
    if with_final_validate:
        FINAL_VALIDATE_SCHEMA(result)
    return result


@pytest.mark.parametrize(
    ("config", "error_match"),
    [
        pytest.param(
            "a string",
            "expected a dictionary",
            id="invalid_string_config",
        ),
        pytest.param(
            {"id": "display_id"},
            r"required key not provided @ data\['model'\]",
            id="missing_model",
        ),
        pytest.param(
            {
                "id": "display_id",
                "model": "ssd1677",
                "dimensions": {"width": 200, "height": 200},
            },
            r"required key not provided @ data\['dc_pin'\]",
            id="missing_dc_pin",
        ),
    ],
)
def test_basic_configuration_errors(
    config: str | ConfigType,
    error_match: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test basic configuration validation errors"""

    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    with pytest.raises(cv.Invalid, match=error_match):
        CONFIG_SCHEMA(config)


def test_all_predefined_models(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
    choose_variant_with_pins: Callable[[list], None],
) -> None:
    """Test all predefined epaper models validate successfully with appropriate defaults."""

    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    # Configure SPI component which is required by epaper_spi
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    # Test all models, providing default values where necessary
    for name, model in MODELS.items():
        config = {"model": name}

        # Add ID field
        config["id"] = "test_display"

        # Add required fields that don't have defaults
        # Use safe GPIO pins that work on ESP32 (avoiding flash pins 6-11)
        if not model.get_default(CONF_DC_PIN):
            config[CONF_DC_PIN] = 21

        # Add dimensions if not provided by model
        if not model.get_default(CONF_WIDTH):
            config[CONF_DIMENSIONS] = {CONF_HEIGHT: 240, CONF_WIDTH: 320}

        # Add init sequence if model doesn't provide one
        if model.initsequence is None:
            config[CONF_INIT_SEQUENCE] = [[0xA0, 0x01]]

        # Add other optional pins that some models might require
        if not model.get_default(CONF_BUSY_PIN):
            config[CONF_BUSY_PIN] = 22

        if not model.get_default(CONF_RESET_PIN):
            config[CONF_RESET_PIN] = 23

        if not model.get_default(CONF_CS_PIN):
            config[CONF_CS_PIN] = 5

        # Dual-CS models (e.g. T133A01) require a second chip-select pin
        if model.manages_cs and not model.get_default("cs1_pin"):
            config["cs1_pin"] = 4

        # Select an ESP32 variant on which all of this model's pins are valid
        # (some models default to high-numbered pins only present on the S3).
        choose_variant_with_pins(_pins_for(model, config))

        run_schema_validation(config)


@pytest.mark.parametrize(
    "model_name",
    [pytest.param(name, id=name.lower()) for name in sorted(MODELS.keys())],
)
def test_individual_models(
    model_name: str,
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
    choose_variant_with_pins: Callable[[list], None],
) -> None:
    """Test each epaper model individually to ensure it validates correctly."""
    model = MODELS[model_name]

    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    # Configure SPI component which is required by epaper_spi
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    config: dict[str, Any] = {"model": model_name, "id": "test_display"}

    # Add required fields based on model defaults
    # Use safe GPIO pins that work on ESP32
    if not model.get_default(CONF_DC_PIN):
        config[CONF_DC_PIN] = 21

    if not model.get_default(CONF_WIDTH):
        config[CONF_DIMENSIONS] = {CONF_HEIGHT: 240, CONF_WIDTH: 320}

    if model.initsequence is None:
        config[CONF_INIT_SEQUENCE] = [[0xA0, 0x01]]

    if not model.get_default(CONF_BUSY_PIN):
        config[CONF_BUSY_PIN] = 22

    if not model.get_default(CONF_RESET_PIN):
        config[CONF_RESET_PIN] = 23

    if not model.get_default(CONF_CS_PIN):
        config[CONF_CS_PIN] = 5

    # Dual-CS models (e.g. T133A01) require a second chip-select pin
    if model.manages_cs and not model.get_default("cs1_pin"):
        config["cs1_pin"] = 4

    # Select an ESP32 variant on which all of this model's pins are valid
    # (some models default to high-numbered pins only present on the S3).
    choose_variant_with_pins(_pins_for(model, config))

    # This should not raise any exceptions
    run_schema_validation(config)


def test_model_with_explicit_dimensions(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Test model configuration with explicitly provided dimensions."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    # Configure SPI component which is required by epaper_spi
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    run_schema_validation(
        {
            "id": "test_display",
            "model": "ssd1677",
            "dc_pin": 21,
            "busy_pin": 22,
            "reset_pin": 23,
            "cs_pin": 5,
            "dimensions": {
                "width": 200,
                "height": 200,
            },
        }
    )


def test_model_with_transform(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Test model configuration with transform options."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    # Configure SPI component which is required by epaper_spi
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    run_schema_validation(
        {
            "id": "test_display",
            "model": "ssd1677",
            "dc_pin": 21,
            "busy_pin": 22,
            "reset_pin": 23,
            "cs_pin": 5,
            "dimensions": {
                "width": 200,
                "height": 200,
            },
            "transform": {
                "mirror_x": True,
                "mirror_y": False,
            },
        }
    )


def test_model_with_full_update_every(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Test model configuration with full_update_every option."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    # Configure SPI component which is required by epaper_spi
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    run_schema_validation(
        {
            "id": "test_display",
            "model": "ssd1677",
            "dc_pin": 21,
            "busy_pin": 22,
            "reset_pin": 23,
            "cs_pin": 5,
            "dimensions": {
                "width": 200,
                "height": 200,
            },
            "full_update_every": 10,
        }
    )


def test_busy_pin_input_mode_ssd1677(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Test that busy_pin has input mode and cs/dc/reset pins have output mode for ssd1677."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    # Configure SPI component which is required by epaper_spi
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    result = run_schema_validation(
        {
            "id": "test_display",
            "model": "ssd1677",
            "dc_pin": 21,
            "busy_pin": 22,
            "reset_pin": 23,
            "cs_pin": 5,
            "dimensions": {
                "width": 200,
                "height": 200,
            },
        }
    )

    # Verify that busy_pin has input mode set
    assert CONF_BUSY_PIN in result
    busy_pin_config = result[CONF_BUSY_PIN]
    assert "mode" in busy_pin_config
    assert busy_pin_config["mode"]["input"] is True

    # Verify that cs_pin has output mode set
    assert CONF_CS_PIN in result
    cs_pin_config = result[CONF_CS_PIN]
    assert "mode" in cs_pin_config
    assert cs_pin_config["mode"]["output"] is True

    # Verify that dc_pin has output mode set
    assert CONF_DC_PIN in result
    dc_pin_config = result[CONF_DC_PIN]
    assert "mode" in dc_pin_config
    assert dc_pin_config["mode"]["output"] is True

    # Verify that reset_pin has output mode set
    assert CONF_RESET_PIN in result
    reset_pin_config = result[CONF_RESET_PIN]
    assert "mode" in reset_pin_config
    assert reset_pin_config["mode"]["output"] is True


def test_enable_pin_single(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Test that a single enable_pin is accepted and normalised to a list of output pins."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    # Configure SPI component which is required by epaper_spi
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    result = run_schema_validation(
        {
            "id": "test_display",
            "model": "ssd1677",
            "dc_pin": 21,
            "busy_pin": 22,
            "reset_pin": 23,
            "cs_pin": 5,
            "enable_pin": 25,
            "dimensions": {
                "width": 200,
                "height": 200,
            },
        }
    )

    # A single pin is normalised to a list by cv.ensure_list
    assert CONF_ENABLE_PIN in result
    enable_pins = result[CONF_ENABLE_PIN]
    assert isinstance(enable_pins, list)
    assert len(enable_pins) == 1
    # enable pins are configured as outputs
    assert enable_pins[0]["mode"]["output"] is True


def test_enable_pin_multiple(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Test that a list of enable_pins is accepted."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    # Configure SPI component which is required by epaper_spi
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    result = run_schema_validation(
        {
            "id": "test_display",
            "model": "ssd1677",
            "dc_pin": 21,
            "busy_pin": 22,
            "reset_pin": 23,
            "cs_pin": 5,
            "enable_pin": [25, 26],
            "dimensions": {
                "width": 200,
                "height": 200,
            },
        }
    )

    assert CONF_ENABLE_PIN in result
    enable_pins = result[CONF_ENABLE_PIN]
    assert isinstance(enable_pins, list)
    assert len(enable_pins) == 2
    assert all(pin["mode"]["output"] is True for pin in enable_pins)


def test_enable_pin_code_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that enable_pins are wired up in the generated C++ code."""
    main_cpp = generate_main(component_config_path("enable_pin_test.yaml"))

    # Derive the auto-generated pin variable names from the set_pin() lines
    # rather than hard-coding them, so the test does not break when unrelated
    # codegen details shift the generated IDs.
    def pin_var_for(gpio_num: int) -> str:
        match = re.search(rf"(\w+)->set_pin\(::GPIO_NUM_{gpio_num}\);", main_cpp)
        assert match is not None, (
            f"GPIO_NUM_{gpio_num} pin not set up in generated code"
        )
        return match.group(1)

    pin_25 = pin_var_for(25)
    pin_26 = pin_var_for(26)

    # Both pin objects must be passed to the display via set_enable_pins() as a
    # std::vector initializer list, in the configured order.
    assert f"set_enable_pins({{{pin_25}, {pin_26}}});" in main_cpp
