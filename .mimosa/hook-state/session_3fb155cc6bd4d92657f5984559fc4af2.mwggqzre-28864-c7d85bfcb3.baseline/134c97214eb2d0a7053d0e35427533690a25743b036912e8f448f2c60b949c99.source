"""Tests for display metadata created by the epaper_spi component."""

from collections.abc import Callable
from pathlib import Path
from typing import Any

from esphome import config_validation as cv
from esphome.components.display import get_all_display_metadata, get_display_metadata
from esphome.components.epaper_spi.display import CONFIG_SCHEMA
from esphome.components.esp32 import KEY_BOARD, KEY_VARIANT, VARIANT_ESP32
from esphome.const import PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def _base_config(**overrides: Any) -> ConfigType:
    """Build a minimal valid ssd1677 config, allowing field overrides."""
    config: ConfigType = {
        "id": "test_display",
        "model": "ssd1677",
        "dc_pin": 21,
        "busy_pin": 22,
        "reset_pin": 23,
        "cs_pin": 5,
        "dimensions": {"width": 200, "height": 300},
    }
    config.update(overrides)
    return config


def test_metadata_dimensions_and_defaults(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Metadata picks up explicit dimensions and epaper_spi defaults."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    config = CONFIG_SCHEMA(_base_config())
    meta = get_display_metadata(config["id"])

    assert meta is not None
    assert meta.width == 200
    assert meta.height == 300
    # epaper_spi always reports full hardware rotation
    assert meta.has_hardware_rotation is True
    # epaper_spi does not declare a byte order
    assert meta.byte_order is cv.UNDEFINED
    assert meta.draw_rounding == 0
    # no drawing methods configured -> no writer
    assert meta.has_writer is False


def test_metadata_default_dimensions_from_model(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """A model with built-in dimensions reports those without explicit dimensions."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    # waveshare-4.26in is an ssd1677 derivative with default 800x480 dimensions
    config = CONFIG_SCHEMA(
        {
            "id": "wave_display",
            "model": "waveshare-4.26in",
            "dc_pin": 21,
            "busy_pin": 22,
            "reset_pin": 23,
            "cs_pin": 5,
        }
    )
    meta = get_display_metadata(config["id"])

    assert meta is not None
    assert meta.width == 800
    assert meta.height == 480


def test_metadata_has_writer_with_auto_clear(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """A display with auto_clear_enabled reports has_writer=True."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    config = CONFIG_SCHEMA(_base_config(auto_clear_enabled=True))
    meta = get_display_metadata(config["id"])

    assert meta is not None
    assert meta.has_writer is True


def test_metadata_rotation_propagated(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """The configured rotation is stored in the metadata."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    config = CONFIG_SCHEMA(_base_config(rotation=90))
    meta = get_display_metadata(config["id"])

    assert meta is not None
    assert meta.rotation == 90


def test_metadata_multiple_displays_independent(
    set_core_config: SetCoreConfigCallable,
    set_component_config: Callable[[str, Any], None],
) -> None:
    """Each display gets its own independent metadata entry."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    set_component_config("spi", {"id": "spi_bus", "clk_pin": 18, "mosi_pin": 19})

    CONFIG_SCHEMA(_base_config(id="disp_a", dimensions={"width": 200, "height": 300}))
    CONFIG_SCHEMA(_base_config(id="disp_b", dimensions={"width": 400, "height": 480}))

    all_meta = get_all_display_metadata()
    assert all_meta["disp_a"].width == 200
    assert all_meta["disp_a"].height == 300
    assert all_meta["disp_b"].width == 400
    assert all_meta["disp_b"].height == 480


def test_metadata_via_code_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Full code generation registers metadata for the configured display."""
    generate_main(component_config_path("enable_pin_test.yaml"))

    all_meta = get_all_display_metadata()
    assert len(all_meta) == 1
    meta = next(iter(all_meta.values()))
    # enable_pin_test.yaml: ssd1677 at 200x200
    assert meta.width == 200
    assert meta.height == 200
    assert meta.has_hardware_rotation is True
