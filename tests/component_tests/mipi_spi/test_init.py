"""Tests for mpip_spi configuration validation."""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)
from esphome.components.mipi import CONF_NATIVE_HEIGHT
from esphome.components.mipi_spi.display import (
    CONF_BUS_MODE,
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    MODELS,
    dimension_schema,
)
from esphome.const import (
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_HEIGHT,
    CONF_INIT_SEQUENCE,
    CONF_WIDTH,
    PlatformFramework,
)
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def run_schema_validation(config: ConfigType) -> None:
    """Run schema validation on a configuration."""
    FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA(config))


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
            {"id": "display_id", "model": "custom", "init_sequence": [[0x36, 0x01]]},
            r"required key not provided @ data\['dimensions'\]",
            id="missing_dimensions",
        ),
        pytest.param(
            {
                "model": "custom",
                "dc_pin": 18,
                "dimensions": {"width": 320, "height": 240},
            },
            r"required key not provided @ data\['init_sequence'\]",
            id="missing_init_sequence",
        ),
        pytest.param(
            {
                "id": "display_id",
                "model": "custom",
                "dimensions": {"width": 320, "height": 240},
                "draw_rounding": 13,
                "init_sequence": [[0xA0, 0x01]],
            },
            r"value must be a power of two for dictionary value @ data\['draw_rounding'\]",
            id="invalid_draw_rounding",
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
        run_schema_validation(config)


@pytest.mark.parametrize(
    ("rounding", "config", "error_match"),
    [
        pytest.param(
            4,
            {"width": 320},
            r"required key not provided @ data\['height'\]",
            id="missing_height",
        ),
        pytest.param(
            32,
            {"width": 320, "height": 111},
            "Dimensions and offsets must be divisible by 32",
            id="dimensions_not_divisible",
        ),
    ],
)
def test_dimension_validation(
    rounding: int,
    config: ConfigType,
    error_match: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test dimension-related validation errors"""

    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    with pytest.raises(cv.Invalid, match=error_match):
        dimension_schema(rounding)(config)


@pytest.mark.parametrize(
    ("config", "error_match"),
    [
        pytest.param(
            {
                "model": "JC3248W535",
                "transform": {"mirror_x": False, "mirror_y": True, "swap_xy": True},
            },
            "Axis swapping not supported by this model",
            id="axis_swapping_not_supported",
        ),
        pytest.param(
            {
                "model": "custom",
                "dimensions": {"width": 320, "height": 240},
                "transform": {"mirror_x": False, "mirror_y": True, "swap_xy": False},
                "init_sequence": [[0x36, 0x01]],
            },
            r"transform is not supported when MADCTL \(0X36\) is in the init sequence",
            id="transform_with_madctl",
        ),
        pytest.param(
            {
                "model": "custom",
                "dimensions": {"width": 320, "height": 240},
                "init_sequence": [[0x3A, 0x01]],
            },
            r"PIXFMT \(0X3A\) should not be in the init sequence, it will be set automatically",
            id="pixfmt_in_init_sequence",
        ),
    ],
)
def test_transform_and_init_sequence_errors(
    config: ConfigType,
    error_match: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test transform and init sequence validation errors"""

    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    with pytest.raises(cv.Invalid, match=error_match):
        run_schema_validation(config)


@pytest.mark.parametrize(
    ("config", "error_match"),
    [
        pytest.param(
            {"model": "t4-s3", "dc_pin": 18},
            "DC pin is not supported in quad mode",
            id="dc_pin_not_supported_quad_mode",
        ),
        pytest.param(
            {"model": "t4-s3", "color_depth": 18},
            "Unknown value '18', valid options are '16', '16bit",
            id="invalid_color_depth_t4_s3",
        ),
        pytest.param(
            {"model": "t-embed", "color_depth": 24},
            "Unknown value '24', valid options are '16', '8",
            id="invalid_color_depth_t_embed",
        ),
        pytest.param(
            {"model": "ili9488"},
            "DC pin is required in single mode",
            id="dc_pin_required_single_mode",
        ),
        pytest.param(
            {"model": "wt32-sc01-plus", "brightness": 128},
            r"extra keys not allowed @ data\['brightness'\]",
            id="brightness_not_supported",
        ),
        pytest.param(
            {"model": "T-DISPLAY-S3-PRO"},
            "PSRAM is required for this display",
            id="psram_required",
        ),
    ],
)
def test_esp32s3_specific_errors(
    config: ConfigType,
    error_match: str,
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test ESP32-S3 specific configuration errors"""

    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32S3},
    )

    with pytest.raises(cv.Invalid, match=error_match):
        run_schema_validation(config)


def test_framework_specific_errors(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test framework-specific configuration errors"""

    set_core_config(
        PlatformFramework.ESP32_ARDUINO,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )

    with pytest.raises(
        cv.Invalid,
        match=r"This feature is only available with framework\(s\) esp-idf",
    ):
        run_schema_validation({"model": "wt32-sc01-plus"})


def test_custom_model_with_all_options(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Test custom model configuration with all available options."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32S3},
    )

    run_schema_validation(
        {
            "model": "custom",
            "pixel_mode": "18bit",
            "color_depth": 8,
            "id": "display_id",
            "byte_order": "little_endian",
            "bus_mode": "single",
            "color_order": "rgb",
            "dc_pin": 11,
            "reset_pin": 12,
            "enable_pin": 13,
            "cs_pin": 14,
            "init_sequence": [[0xA0, 0x01]],
            "dimensions": {
                "width": 320,
                "height": 240,
                "offset_width": 32,
                "offset_height": 32,
            },
            "invert_colors": True,
            "transform": {"mirror_x": True, "mirror_y": True, "swap_xy": False},
            "spi_mode": "mode0",
            "data_rate": "40MHz",
            "use_axis_flips": True,
            "draw_rounding": 4,
            "spi_16": True,
            "buffer_size": 0.25,
        }
    )


def test_all_predefined_models(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
    choose_variant_with_pins: Callable[[list], None],
) -> None:
    """Test all predefined display models validate successfully with appropriate defaults."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32S3},
    )

    # Enable PSRAM which is required for some models
    set_component_config("psram", True)

    # Test all models, providing default values where necessary
    for name, model in MODELS.items():
        config = {"model": name}

        # Get the pins required by this model and find a compatible variant
        pins = [
            pin
            for pin in [
                model.get_default(pin, None)
                for pin in ("dc_pin", "reset_pin", "cs_pin")
            ]
            if pin is not None
        ]
        choose_variant_with_pins(pins)

        # Add required fields that don't have defaults
        if (
            not model.get_default(CONF_DC_PIN)
            and model.get_default(CONF_BUS_MODE) != "quad"
        ):
            config[CONF_DC_PIN] = 14
        if not model.get_default(CONF_NATIVE_HEIGHT):
            config[CONF_DIMENSIONS] = {CONF_HEIGHT: 240, CONF_WIDTH: 320}
        if model.initsequence is None:
            config[CONF_INIT_SEQUENCE] = [[0xA0, 0x01]]

        run_schema_validation(config)


def test_native_generation(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
) -> None:
    """Test code generation for display."""

    main_cpp = generate_main(component_fixture_path("native.yaml"))
    assert (
        "mipi_spi::MipiSpiBuffer<uint16_t, mipi_spi::PIXEL_MODE_16, true, mipi_spi::PIXEL_MODE_16, mipi_spi::BUS_TYPE_QUAD, 360, 360, 0, 1, display::DISPLAY_ROTATION_0_DEGREES, 1>()"
        in main_cpp
    )
    assert "set_init_sequence({240, 1, 8, 242" in main_cpp
    assert "show_test_card();" in main_cpp
    assert "set_write_only(true);" in main_cpp


def test_lvgl_generation(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
) -> None:
    """Test LVGL generation configuration."""

    main_cpp = generate_main(component_fixture_path("lvgl.yaml"))
    assert (
        "mipi_spi::MipiSpi<uint16_t, mipi_spi::PIXEL_MODE_16, true, mipi_spi::PIXEL_MODE_16, mipi_spi::BUS_TYPE_SINGLE, 128, 160, 0, 0>();"
        in main_cpp
    )
    assert "set_init_sequence({1, 0, 10, 255, 177" in main_cpp
    assert "show_test_card();" not in main_cpp
    assert "set_auto_clear(false);" in main_cpp
