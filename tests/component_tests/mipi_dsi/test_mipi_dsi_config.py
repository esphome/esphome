"""Tests for mpi_dsi configuration validation."""

from collections.abc import Callable
import logging
from pathlib import Path

import pytest

from esphome import config_validation as cv
from esphome.components.esp32 import KEY_BOARD, VARIANT_ESP32P4

# Importing xl9535 registers its pin schema with pins.PIN_SCHEMA_REGISTRY so that
# models (e.g. SEEED-RETERMINAL-D1001) that reference xl9535-backed pins in their
# defaults can be validated by the mipi_dsi CONFIG_SCHEMA in this test.
import esphome.components.xl9535  # noqa: F401
from esphome.const import (
    CONF_DIMENSIONS,
    CONF_HEIGHT,
    CONF_INIT_SEQUENCE,
    CONF_WIDTH,
    KEY_VARIANT,
    PlatformFramework,
)
from tests.component_tests.types import SetCoreConfigCallable


def test_configuration_errors(set_core_config: SetCoreConfigCallable) -> None:
    """Test detection of invalid configuration"""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-p4-evboard", KEY_VARIANT: VARIANT_ESP32P4},
    )

    from esphome.components.mipi_dsi.display import CONFIG_SCHEMA

    with pytest.raises(cv.Invalid, match="expected a dictionary"):
        CONFIG_SCHEMA("a string")

    with pytest.raises(
        cv.Invalid, match=r"required key not provided @ data\['model'\]"
    ):
        CONFIG_SCHEMA({"id": "display_id"})

    with pytest.raises(
        cv.Invalid,
        match=r"string value is None for dictionary value @ data\['lane_bit_rate'\]",
    ):
        CONFIG_SCHEMA(
            {"id": "display_id", "model": "custom", "init_sequence": [[0x36, 0x01]]}
        )

    with pytest.raises(
        cv.Invalid, match=r"required key not provided @ data\['dimensions'\]"
    ):
        CONFIG_SCHEMA(
            {
                "id": "display_id",
                "model": "custom",
                "init_sequence": [[0x36, 0x01]],
                "lane_bit_rate": "1.5Gbps",
            }
        )

    with pytest.raises(
        cv.Invalid, match=r"required key not provided @ data\['init_sequence'\]"
    ):
        CONFIG_SCHEMA(
            {
                "model": "custom",
                "lane_bit_rate": "1.5Gbps",
                "dimensions": {"width": 320, "height": 240},
            }
        )

    # DSI displays cannot swap axes; enabling swap_xy reports a clear error.
    with pytest.raises(cv.Invalid, match="'swap_xy' is not supported by this model"):
        CONFIG_SCHEMA(
            {
                "model": "custom",
                "init_sequence": [[0xA0, 0x01]],
                "lane_bit_rate": "1.5Gbps",
                "dimensions": {"width": 320, "height": 240},
                "transform": {"mirror_x": True, "mirror_y": True, "swap_xy": True},
            }
        )


def test_configuration_success(set_core_config: SetCoreConfigCallable) -> None:
    """Test successful configuration validation."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-p4-evboard", KEY_VARIANT: VARIANT_ESP32P4},
    )

    from esphome.components.mipi_dsi.display import CONFIG_SCHEMA, MODELS

    # Custom model with all options
    CONFIG_SCHEMA(
        {
            "model": "custom",
            "pixel_mode": "16bit",
            "id": "display_id",
            "byte_order": "little_endian",
            "color_order": "rgb",
            "reset_pin": 12,
            "init_sequence": [[0xA0, 0x01]],
            "dimensions": {
                "width": 320,
                "height": 240,
            },
            "invert_colors": True,
            "transform": {"mirror_x": True, "mirror_y": True},
            "pclk_frequency": "40MHz",
            "lane_bit_rate": "1.5Gbps",
            "lanes": 2,
            "use_axis_flips": True,
        }
    )

    # Test all models, providing default values where necessary
    for name, model in MODELS.items():
        config = {"model": name}
        if model.initsequence is None:
            config[CONF_INIT_SEQUENCE] = [[0xA0, 0x01]]
        if not model.get_default(CONF_DIMENSIONS):
            config[CONF_DIMENSIONS] = {CONF_WIDTH: 400, CONF_HEIGHT: 300}
        if not model.get_default("lane_bit_rate"):
            config["lane_bit_rate"] = "1.5Gbps"
        CONFIG_SCHEMA(config)


def test_deprecated_model_warning(
    set_core_config: SetCoreConfigCallable,
    caplog: pytest.LogCaptureFixture,
) -> None:
    """The deprecated M5Stack-Tab5-v2 alias warns and points at the replacement models."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-p4-evboard", KEY_VARIANT: VARIANT_ESP32P4},
    )

    from esphome.components.mipi_dsi.display import CONFIG_SCHEMA

    with caplog.at_level(logging.WARNING):
        CONFIG_SCHEMA({"id": "deprecated_display", "model": "M5Stack-Tab5-v2"})
    assert "M5STACK-TAB5-V2 is deprecated" in caplog.text
    # The warning names the replacement models so users know what to switch to.
    assert "M5STACK-TAB5-ST7123" in caplog.text

    # The replacement models validate without emitting a deprecation warning.
    caplog.clear()
    with caplog.at_level(logging.WARNING):
        CONFIG_SCHEMA({"id": "st7123_display", "model": "M5Stack-Tab5-ST7123"})
        CONFIG_SCHEMA({"id": "st7121_display", "model": "M5Stack-Tab5-ST7121"})
    assert "deprecated" not in caplog.text


def test_metadata_records_rotation(set_core_config: SetCoreConfigCallable) -> None:
    """A configured display rotation is recorded in the metadata.

    LVGL relies on this to flag a rotation set in the display config (see the
    mipi_spi tests for the end-to-end LVGL rejection).
    """
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-p4-evboard", KEY_VARIANT: VARIANT_ESP32P4},
    )

    from esphome.components.display import get_display_metadata
    from esphome.components.mipi_dsi.display import CONFIG_SCHEMA

    base = {
        "model": "custom",
        "init_sequence": [[0xA0, 0x01]],
        "lane_bit_rate": "1.5Gbps",
        "dimensions": {"width": 320, "height": 240},
    }
    config = CONFIG_SCHEMA({**base, "id": "rotated", "rotation": 90})
    assert get_display_metadata(config["id"]).rotation == 90

    config = CONFIG_SCHEMA({**base, "id": "unrotated"})
    assert get_display_metadata(config["id"]).rotation == 0


def test_code_generation(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
) -> None:
    """Test code generation for display."""

    main_cpp = generate_main(component_fixture_path("mipi_dsi.yaml"))
    assert (
        "alignas(mipi_dsi::MipiDsi) static unsigned char mipi_dsi__p4_nano__pstorage[sizeof(mipi_dsi::MipiDsi)];"
        in main_cpp
    )
    assert (
        "static mipi_dsi::MipiDsi *const p4_nano = reinterpret_cast<mipi_dsi::MipiDsi *>(mipi_dsi__p4_nano__pstorage);"
        in main_cpp
    )
    assert (
        "new(p4_nano) mipi_dsi::MipiDsi(800, 1280, display::COLOR_BITNESS_565, 16);"
        in main_cpp
    )
    assert "set_init_sequence({224, 1, 0, 225, 1, 147, 226, 1," in main_cpp
    assert "p4_nano->set_lane_bit_rate(1500.0f);" in main_cpp
    assert "p4_nano->set_rotation(display::DISPLAY_ROTATION_90_DEGREES);" in main_cpp
    assert "p4_86->set_rotation(display::DISPLAY_ROTATION_0_DEGREES);" not in main_cpp
    assert "custom_id->set_rotation(display::DISPLAY_ROTATION_180_DEGREES);" in main_cpp
    # assert "backlight_id = new light::LightState(mipi_dsi_dsibacklight_id);" in main_cpp
