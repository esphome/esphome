"""Tests for epaper_spi configuration validation."""

from collections.abc import Callable
from pathlib import Path
import re
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.const import CONF_HOLD_STATE
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
from esphome.components.esp8266 import gpio as esp8266_gpio  # noqa: F401  registers the pin schema
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
            config[CONF_DIMENSIONS] = {CONF_HEIGHT: 120, CONF_WIDTH: 120}

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
        config[CONF_DIMENSIONS] = {CONF_HEIGHT: 120, CONF_WIDTH: 120}

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


def test_update_interval_below_model_minimum_rejected(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """update_interval faster than the model's minimum_update_interval is rejected."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    with pytest.raises(cv.Invalid, match="at least"):
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
                "update_interval": "500ms",
            }
        )


def test_reset_duration_over_max_rejected(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """reset_duration over the 500ms cap is rejected."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    with pytest.raises(cv.Invalid, match="at most"):
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
                "reset_duration": "600ms",
            }
        )


def test_ssd1681_defaults_and_driver_class(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The SSD1681 models default to 200x200 and all use the SSD1681 driver class."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    for name in ("ssd1681", "goodisplay-gdey0154d67-1.54", "waveshare-1.54in-v2"):
        model = MODELS[name.upper()]
        assert model.class_name == "EPaperSSD1681"
        config = run_schema_validation(
            {
                "id": "test_display",
                "model": name,
                "dc_pin": 21,
                "reset_pin": 23,
                "full_update_every": 20,
            }
        )
        assert model.get_dimensions(config) == (200, 200)


def test_ssd1681_needs_reset_pin(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The SSD1681 sleeps after every update and only a hardware reset wakes it, so reset_pin is required."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    for full_update_every in (1, 20):
        with pytest.raises(cv.Invalid, match="reset_pin"):
            run_schema_validation(
                {
                    "id": "test_display",
                    "model": "ssd1681",
                    "dc_pin": 21,
                    "full_update_every": full_update_every,
                }
            )


def test_ssd1681_dimensions_over_controller_limit_rejected(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Dimensions larger than the SSD1681 can drive are rejected."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    with pytest.raises(cv.Invalid, match="at most 200x200"):
        run_schema_validation(
            {
                "id": "test_display",
                "model": "ssd1681",
                "dc_pin": 21,
                "dimensions": {
                    "width": 400,
                    "height": 300,
                },
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


def test_uc8179_e1001_code_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that the reTerminal E1001 model generates the UC8179 driver and init sequence."""
    main_cpp = generate_main(component_config_path("uc8179_e1001_test.yaml"))

    # The model must instantiate the UC8179 driver class with the panel dimensions
    assert "epaper_spi::EPaperUC8179" in main_cpp
    assert re.search(r'"SEEED-RETERMINAL-E1001",\s*800,\s*480', main_cpp)

    # The generated init sequence must contain the UC8179 resolution setting
    # for 800x480: command 0x61, 4 data bytes 0x03 0x20 0x01 0xE0
    # (rendered as decimal in the generated array)
    assert "97, 4, 3, 32, 1, 224" in main_cpp


def test_uc8279_width_must_be_a_multiple_of_8(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The UC8279 takes whole bytes of sources, so the width is a multiple of 8."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    with pytest.raises(cv.Invalid, match="multiple of 8"):
        run_schema_validation(
            {
                "id": "test_display",
                "model": "uc8279",
                "dc_pin": 21,
                "dimensions": {"width": 804, "height": 480},
            }
        )


GDEY029Z95 = "goodisplay-gdey029z95-2.9"


def _gdey029z95_config(**overrides: Any) -> ConfigType:
    config: ConfigType = {
        "id": "test_display",
        "model": GDEY029Z95,
        CONF_DC_PIN: 21,
        CONF_RESET_PIN: 22,
        CONF_BUSY_PIN: 23,
    }
    config.update(overrides)
    return config


def test_gdey029z95_uses_the_good_display_driver() -> None:
    """The GDEY029Z95 uses EPaperGoodDisplay3C with the WeAct init sequence."""
    model = MODELS[GDEY029Z95.upper()]

    assert model.class_name == "EPaperGoodDisplay3C"
    assert model.get_init_sequence({}) == (
        (0x01, 0x27, 0x01, 0x00),
        (0x11, 0x03),
        (0x3C, 0x05),
        (0x18, 0x80),
        (0x21, 0x00, 0x80),
    )


@pytest.mark.parametrize("pin", [CONF_RESET_PIN, CONF_BUSY_PIN])
def test_gdey029z95_requires_reset_and_busy_pins(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
    pin: str,
) -> None:
    """Only a hardware reset wakes the panel from deep sleep, and refreshes are timed by busy."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    config = _gdey029z95_config()
    del config[pin]
    with pytest.raises(cv.Invalid, match=pin):
        run_schema_validation(config)


def test_gdey029z95_has_no_partial_update(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    run_schema_validation(_gdey029z95_config())
    with pytest.raises(cv.Invalid, match="does not support partial update"):
        run_schema_validation(_gdey029z95_config(full_update_every=2))


def test_uc8279_x4pro_code_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The Xteink X4 Pro generates the UC8279 driver with the panel's gate offset and gate count."""
    main_cpp = generate_main(component_config_path("uc8279_x4pro_test.yaml"))

    assert "epaper_spi::EPaperUC8279" in main_cpp
    # 800 sources, 480 rows, and the panel on the last 480 of 600 gates
    assert re.search(
        r'"XTEINK-X4PRO-UC8279",\s*800,\s*480,.*?,\s*120\)', main_cpp, re.DOTALL
    )
    resolution = next(
        cmd
        for cmd in MODELS["XTEINK-X4PRO-UC8279"].get_init_sequence({})
        if cmd[0] == 0x61
    )
    assert resolution == (0x61, 0x03, 0x20, 0x02, 0x58)


def test_uc8279_offset_height_configurable(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The generic uc8279 model takes offset_height under dimensions, reaching the constructor and gate count."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    config = run_schema_validation(
        {
            "id": "test_display",
            "model": "uc8279",
            "dc_pin": 21,
            "dimensions": {"width": 16, "height": 2, "offset_height": 8},
        }
    )
    model = MODELS["UC8279"]
    assert model.get_constructor_args(config) == (8,)
    resolution = next(cmd for cmd in model.get_init_sequence(config) if cmd[0] == 0x61)
    # 16 sources, and 8 + 2 = 10 gates
    assert resolution == (0x61, 0x00, 0x10, 0x00, 0x0A)


def test_uc8279_offset_height_plus_height_must_fit_16_bits(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """offset_height plus the panel height is the gate count, which the controller holds in 16 bits."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    with pytest.raises(cv.Invalid, match="must not exceed"):
        run_schema_validation(
            {
                "id": "test_display",
                "model": "uc8279",
                "dc_pin": 21,
                "dimensions": {"width": 16, "height": 2, "offset_height": 0xFFFF},
            }
        )


def test_uc8279_x4pro_explicit_dimensions_keep_the_model_offset(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Writing out the X4 Pro's dimensions without offset_height keeps the panel's offset of 120."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 12, "mosi_pin": 11})

    config = run_schema_validation(
        {
            "id": "test_display",
            "model": "xteink-x4pro-uc8279",
            "dimensions": {"width": 800, "height": 480},
        }
    )
    assert MODELS["XTEINK-X4PRO-UC8279"].get_constructor_args(config) == (120,)


def test_offset_height_rejected_by_models_without_it(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """A model that does not use offset_height rejects it rather than ignoring it."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    with pytest.raises(cv.Invalid, match="offset_height"):
        run_schema_validation(
            {
                "id": "test_display",
                "model": "ssd1677",
                "dc_pin": 21,
                "dimensions": {"width": 200, "height": 200, "offset_height": 8},
            }
        )


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


def test_full_update_next_action_code_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The epaper_spi.full_update_next action targets the configured display."""
    main_cpp = generate_main(component_config_path("full_update_next_test.yaml"))

    assert "epaper_display->request_full_update();" in main_cpp


def test_sleep_state_code_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """partial_update_after_deep_sleep gives the display a sleep state slot in RTC memory."""
    main_cpp = generate_main(component_config_path("resume_after_deep_sleep_test.yaml"))

    assert "epaper_display->set_sleep_state_hash(" in main_cpp


def test_no_sleep_state_by_default(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """By default every wake refreshes fully and nothing is kept across the sleep."""
    main_cpp = generate_main(component_config_path("full_update_next_test.yaml"))

    assert "set_sleep_state_hash(" not in main_cpp


def test_partial_update_after_deep_sleep_needs_partial_updates(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """partial_update_after_deep_sleep is pointless without partial updates."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    with pytest.raises(cv.Invalid, match="full_update_every"):
        run_schema_validation(
            {
                "id": "test_display",
                "model": "ssd1681",
                "dc_pin": 21,
                "reset_pin": 23,
                "partial_update_after_deep_sleep": "panel",
            }
        )


def test_partial_update_after_deep_sleep_holds_the_enable_pins(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """With the panel keeping its image, its enable pins hold their level through the deep sleep."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    base = {
        "id": "test_display",
        "model": "ssd1681",
        "dc_pin": 21,
        "reset_pin": 23,
        "enable_pin": [{"number": 25, "inverted": True}, 26],
        "full_update_every": 20,
    }
    config = run_schema_validation(base)
    assert not any(pin[CONF_HOLD_STATE] for pin in config[CONF_ENABLE_PIN])

    config = run_schema_validation({**base, "partial_update_after_deep_sleep": "panel"})
    assert all(pin[CONF_HOLD_STATE] for pin in config[CONF_ENABLE_PIN])


def test_partial_update_after_deep_sleep_rejects_esp8266_enable_pins(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The ESP8266 cannot keep a panel powered through an enable pin while it sleeps."""
    set_core_config(
        PlatformFramework.ESP8266_ARDUINO, platform_data={KEY_BOARD: "d1_mini"}
    )

    set_component_config("spi", {"id": "spi_bus", "clk_pin": 14, "mosi_pin": 13})

    base = {
        "id": "test_display",
        "model": "ssd1681",
        "dc_pin": 4,
        "reset_pin": 2,
        "full_update_every": 20,
        "partial_update_after_deep_sleep": "panel",
    }
    run_schema_validation(base)

    with pytest.raises(cv.Invalid, match="cannot hold the enable pins"):
        run_schema_validation({**base, "enable_pin": 5})


def test_partial_update_after_deep_sleep_needs_a_model_that_supports_it(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Models whose driver cannot resume after a deep sleep reject the option."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    with pytest.raises(cv.Invalid, match="does not support"):
        run_schema_validation(
            {
                "id": "test_display",
                "model": "ssd1683",
                "dc_pin": 21,
                "dimensions": {"width": 200, "height": 200},
                "full_update_every": 20,
                "partial_update_after_deep_sleep": "panel",
            }
        )


def test_is_updating_condition_code_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The epaper_spi.is_updating condition checks the configured display."""
    main_cpp = generate_main(component_config_path("full_update_next_test.yaml"))

    assert "epaper_display->is_updating()" in main_cpp


def test_model_with_no_default_init_sequence_generates(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that code generation succeeds for a model with no default init sequence.

    The base "t133a01" model (used directly, not via one of its `.extend()`
    variants) doesn't override `get_init_sequence()` or pass `initsequence` to
    its constructor, and the user didn't supply `init_sequence:` either.
    `EpaperModel.get_init_sequence()` used to default to `None` in this case,
    which made `flatten_sequence()` raise a `TypeError` during code
    generation. Regression test for that crash.
    """
    main_cpp = generate_main(component_config_path("t133a01_no_init_sequence.yaml"))

    # The generated constructor call takes (name, width, height, init_sequence,
    # init_sequence_length, ...); a length of 0 confirms the empty init
    # sequence array was generated instead of raising during code generation.
    assert re.search(r"epaper_spi::EPaperT133A01\([^;]*,\s*\w+,\s*0\);", main_cpp)


def test_test_card_overrides_update_interval(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """With nothing else to draw, update_interval is not left at never for the test card."""
    main_cpp = generate_main(component_config_path("test_card_update_interval.yaml"))

    match = re.search(r"epaper_display->set_update_interval\((\d+)\);", main_cpp)
    assert match is not None
    assert int(match.group(1)) != 4294967295
