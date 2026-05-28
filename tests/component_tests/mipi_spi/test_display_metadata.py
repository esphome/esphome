"""Tests for display metadata created by mipi_spi component."""

from collections.abc import Callable
from pathlib import Path

from esphome.components.const import BYTE_ORDER_BIG
from esphome.components.display import get_all_display_metadata, get_display_metadata
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)
from esphome.components.mipi_spi.display import CONFIG_SCHEMA, FINAL_VALIDATE_SCHEMA
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def validated_config(config):
    """Run schema + final validation and return the validated config."""
    config = CONFIG_SCHEMA(config)
    FINAL_VALIDATE_SCHEMA(config)
    return config


def test_metadata_native_quad_default_test_card(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A quad-mode display with no explicit drawing gets a test card from final validation."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    config = CONFIG_SCHEMA({"model": "JC3636W518", "id": "jc3232w518"})
    meta = get_display_metadata(config["id"])
    assert meta is not None
    assert meta.width == 360
    assert meta.height == 360
    assert meta.has_hardware_rotation is True
    assert meta.byte_order == BYTE_ORDER_BIG


def test_metadata_single_mode_with_dc_pin(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A single-mode display with no explicit drawing gets metadata from schema validation."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    config = CONFIG_SCHEMA(
        {"model": "ST7735", "dc_pin": 18, "id": "single_mode_with_dc_pin"}
    )
    meta = get_display_metadata(config["id"])
    assert meta is not None
    assert meta.width == 128
    assert meta.height == 160
    assert meta.has_hardware_rotation is True
    assert meta.byte_order == BYTE_ORDER_BIG


def test_metadata_custom_dimensions(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A custom model picks up explicit dimensions."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    config = CONFIG_SCHEMA(
        {
            "model": "custom",
            "dc_pin": 18,
            "dimensions": {"width": 480, "height": 320},
            "init_sequence": [[0xA0, 0x01]],
            "id": "custom_dimensions",
        }
    )
    meta = get_display_metadata(config["id"])
    assert meta is not None
    assert meta.width == 480
    assert meta.height == 320
    assert meta.has_hardware_rotation is True


def test_metadata_no_swap_xy_not_full_hardware_rotation(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A model that disables swap_xy should report has_hardware_rotation=False."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    # JC3248W535 has swap_xy=cv.UNDEFINED -> transforms={mirror_x, mirror_y} only
    config = CONFIG_SCHEMA({"model": "JC3248W535", "id": "jc3248w535"})
    meta = get_display_metadata(config["id"])
    assert meta is not None
    assert meta.has_hardware_rotation is False


def test_metadata_multiple_displays_independent(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """Multiple displays each get their own metadata entry."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    CONFIG_SCHEMA(
        {
            "id": "disp_a",
            "model": "custom",
            "dc_pin": 18,
            "dimensions": {"width": 320, "height": 240},
            "init_sequence": [[0xA0, 0x01]],
        }
    )
    CONFIG_SCHEMA(
        {
            "id": "disp_b",
            "model": "custom",
            "dc_pin": 19,
            "dimensions": {"width": 128, "height": 64},
            "init_sequence": [[0xA0, 0x01]],
        }
    )

    all_meta = get_all_display_metadata()
    assert all_meta["disp_a"].width == 320
    assert all_meta["disp_a"].height == 240
    assert all_meta["disp_a"].has_hardware_rotation is True
    assert all_meta["disp_a"].byte_order == BYTE_ORDER_BIG
    assert all_meta["disp_b"].width == 128
    assert all_meta["disp_b"].height == 64
    assert all_meta["disp_b"].has_hardware_rotation is True
    assert all_meta["disp_b"].byte_order == BYTE_ORDER_BIG


def test_metadata_via_code_generation_native(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
) -> None:
    """Full code generation for native.yaml should produce correct metadata."""
    generate_main(component_fixture_path("native.yaml"))
    all_meta = get_all_display_metadata()
    # native.yaml: model JC3636W518 -> 360x360, full hardware rotation
    assert len(all_meta) == 1
    meta = next(iter(all_meta.values()))
    assert meta.width == 360
    assert meta.height == 360
    assert meta.has_hardware_rotation is True
    assert meta.byte_order == BYTE_ORDER_BIG


def test_metadata_via_code_generation_lvgl(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
) -> None:
    """Full code generation for lvgl.yaml should produce correct metadata."""
    generate_main(component_fixture_path("lvgl.yaml"))
    all_meta = get_all_display_metadata()
    # lvgl.yaml: model ST7735 -> 128x160, full hw rotation
    assert len(all_meta) == 1
    meta = next(iter(all_meta.values()))
    assert meta.width == 128
    assert meta.height == 160
    assert meta.has_hardware_rotation is True
    assert meta.byte_order == BYTE_ORDER_BIG
