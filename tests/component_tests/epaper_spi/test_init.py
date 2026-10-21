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


def test_is_updating_condition_code_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The epaper_spi.is_updating condition checks the configured display."""
    main_cpp = generate_main(component_config_path("full_update_next_test.yaml"))

    assert "epaper_display->is_updating()" in main_cpp


def test_t133a01_init_sequence_follows_dimensions(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """The T133A01 init sequence comes from the model, with the resolution taken from the dimensions.

    Code generation for the base "t133a01" model used directly (not via one of its `.extend()`
    variants) also once crashed when it had no init sequence, so this covers that path too.
    """
    main_cpp = generate_main(component_config_path("t133a01_no_init_sequence.yaml"))

    # The generated constructor call takes (name, width, height, init_sequence, init_sequence_length).
    # 16 commands of 2 to 11 bytes plus a 2-byte delay after each.
    match = re.search(r"epaper_spi::EPaperT133A01\([^;]*,\s*\w+,\s*(\d+)\);", main_cpp)
    assert match is not None
    assert int(match.group(1)) > 16 * 4
    # TRES for 200x200: command 0x61, count, chip mask (both), width 200, half the height 100
    assert "97, 5, 3, 0, 200, 0, 100" in main_cpp


def test_test_card_overrides_update_interval(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """With nothing else to draw, update_interval is not left at never for the test card."""
    main_cpp = generate_main(component_config_path("test_card_update_interval.yaml"))

    match = re.search(r"epaper_display->set_update_interval\((\d+)\);", main_cpp)
    assert match is not None
    assert int(match.group(1)) != 4294967295


@pytest.mark.parametrize(
    ("model", "tres"),
    [
        ("inkplate13spectra", (0x61, 3, 0x04, 0xB0, 0x03, 0x20)),
        ("seeed-reterminal-e1004", (0x61, 3, 0x04, 0xB0, 0x03, 0x20)),
        ("t133a01", (0x61, 3, 0x00, 0xC8, 0x00, 0x64)),
    ],
)
def test_dualcs_resolution_follows_dimensions(
    model: str, tres: tuple[int, ...]
) -> None:
    """Each chip gets the full width and half the height; 1200x1600 is Soldered's 1200x800."""
    config: ConfigType = {}
    if model == "t133a01":
        config[CONF_DIMENSIONS] = {CONF_WIDTH: 200, CONF_HEIGHT: 200}
    sequence = MODELS[model.upper()].get_init_sequence(config)
    assert [entry for entry in sequence if entry[0] == 0x61] == [tres]


@pytest.mark.parametrize("model", ["inkplate13spectra", "t133a01"])
def test_dualcs_rejects_custom_init_sequence(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
    model: str,
) -> None:
    """A dual chip select init sequence needs a chip for every command, which the generic option has not."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 38, "mosi_pin": 40})

    config: dict[str, Any] = {
        "id": "test_display",
        "model": model,
        "init_sequence": [[0xA0, 0x01]],
    }
    if model == "t133a01":
        config.update(
            {
                "dc_pin": 14,
                "cs_pin": 42,
                "cs1_pin": 39,
                "dimensions": {"width": 1200, "height": 1600},
            }
        )
    with pytest.raises(cv.Invalid, match="does not support a custom 'init_sequence'"):
        run_schema_validation(config)


def _inkplate13spectra_core(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 38, "mosi_pin": 40})


def test_inkplate13spectra_uses_standard_reset_and_enable_pins(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The board's RST and PWR_EN lines are the standard reset_pin and enable_pin options."""
    _inkplate13spectra_core(set_core_config, set_component_config)

    result = run_schema_validation({"id": "test_display", "model": "inkplate13spectra"})
    assert result["reset_pin"]["number"] == 4
    assert [pin["number"] for pin in result["enable_pin"]] == [21]


def test_inkplate13spectra_rejects_reset_duration(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The driver sets its own reset timing, so a configured reset_duration would be ignored."""
    _inkplate13spectra_core(set_core_config, set_component_config)

    with pytest.raises(cv.Invalid, match="sets its own reset timing"):
        run_schema_validation(
            {
                "id": "test_display",
                "model": "inkplate13spectra",
                "reset_duration": "50ms",
            }
        )


def test_inkplate13spectra_rejects_inverted_expander_pin(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The driver can only drive an inverted pin physically low when it is on the chip itself."""
    import esphome.components.pcf8574  # noqa: F401  # registers the pcf8574 pin schema

    _inkplate13spectra_core(set_core_config, set_component_config)

    with pytest.raises(cv.Invalid, match="Only a pin on the chip itself"):
        run_schema_validation(
            {
                "id": "test_display",
                "model": "inkplate13spectra",
                "enable_pin": {"pcf8574": "expander", "number": 0, "inverted": True},
            }
        )


@pytest.mark.parametrize(
    ("dimensions", "error"),
    [
        ({"width": 1200, "height": 1600}, None),
        ({"width": 1600, "height": 1200}, None),
        ({"width": 1604, "height": 1600}, "supports a width of at most 1600"),
        ({"width": 1202, "height": 1600}, "width that is a multiple of 4"),
        ({"width": 1200, "height": 1601}, "requires an even height"),
    ],
)
def test_t133a01_dimensions_bounded(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
    dimensions: dict[str, int],
    error: str | None,
) -> None:
    """The T133A01 row buffer bounds the width, and its partial window needs aligned dimensions."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 38, "mosi_pin": 40})

    config = {
        "id": "test_display",
        "model": "t133a01",
        "dc_pin": 14,
        "cs_pin": 42,
        "cs1_pin": 39,
        "dimensions": dimensions,
    }
    if error is None:
        run_schema_validation(config)
    else:
        with pytest.raises(cv.Invalid, match=error):
            run_schema_validation(config)


@pytest.mark.parametrize(
    ("model", "accepted"),
    [
        ("inkplate13spectra", True),
        ("t133a01", False),
    ],
)
def test_full_update_every_needs_partial_update_support(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
    model: str,
    accepted: bool,
) -> None:
    """Of the dual chip select panels, only the Inkplate 13 Spectra enables partial update."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 38, "mosi_pin": 40})

    config: dict[str, Any] = {
        "id": "test_display",
        "model": model,
        "dimensions": {"width": 1200, "height": 1600},
        "full_update_every": 2,
    }
    if model == "t133a01":
        config.update(
            {
                "dc_pin": 14,
                "cs_pin": 42,
                "cs1_pin": 39,
                "busy_pin": 7,
                "reset_pin": 4,
            }
        )
    if accepted:
        run_schema_validation(config)
    else:
        with pytest.raises(cv.Invalid, match="does not support partial update"):
            run_schema_validation(config)
