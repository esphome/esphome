"""Tests for epaper_spi configuration validation."""

from collections.abc import Callable
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.epaper_spi.display import (
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    MODELS,
)
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_CS_PIN,
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_HEIGHT,
    CONF_INIT_SEQUENCE,
    CONF_RESET_PIN,
    CONF_WIDTH,
    PlatformFramework,
)
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


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
) -> None:
    """Test all predefined epaper models validate successfully with appropriate defaults."""

    # Test all models, providing default values where necessary
    for name, model in MODELS.items():
        # SEEED models are designed for ESP32-S3 hardware
        if name in ("SEEED-EE04-MONO-4.26", "SEEED-RETERMINAL-E1002"):
            set_core_config(
                PlatformFramework.ESP32_IDF,
                platform_data={
                    KEY_BOARD: "esp32-s3-devkitc-1",
                    KEY_VARIANT: VARIANT_ESP32S3,
                },
            )
        else:
            set_core_config(
                PlatformFramework.ESP32_IDF,
                platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
            )

        # Configure SPI component which is required by epaper_spi
        set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

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

        run_schema_validation(config)


@pytest.mark.parametrize(
    "model_name",
    [pytest.param(name, id=name.lower()) for name in sorted(MODELS.keys())],
)
def test_individual_models(
    model_name: str,
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Test each epaper model individually to ensure it validates correctly."""
    # SEEED models are designed for ESP32-S3 hardware
    if model_name in ("SEEED-EE04-MONO-4.26", "SEEED-RETERMINAL-E1002"):
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={
                KEY_BOARD: "esp32-s3-devkitc-1",
                KEY_VARIANT: VARIANT_ESP32S3,
            },
        )
    else:
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
        )

    # Configure SPI component which is required by epaper_spi
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    model = MODELS[model_name]
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
