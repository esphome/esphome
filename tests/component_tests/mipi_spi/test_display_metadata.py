"""Tests for display metadata created by mipi_spi component."""

from collections.abc import Callable
from pathlib import Path

from esphome.components.display import (
    DisplayMetaData,
    get_all_display_metadata,
    get_display_metadata,
)
from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)
from esphome.components.mipi_spi.display import (
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    get_instance,
)
from esphome.const import PlatformFramework
from tests.component_tests.types import SetCoreConfigCallable


def validated_config(config):
    """Run schema + final validation and return the validated config."""
    return FINAL_VALIDATE_SCHEMA(CONFIG_SCHEMA(config))


def test_metadata_native_quad_default_test_card(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A quad-mode display with no explicit drawing gets a test card from final validation."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    config = validated_config({"model": "JC3636W518"})
    get_instance(config)
    meta = get_display_metadata(str(config["id"]))
    assert meta is not None
    assert meta.width == 360
    assert meta.height == 360
    # final validation auto-enables show_test_card when no drawing methods are configured
    assert meta.has_writer is True
    assert meta.has_hardware_rotation is True


def test_metadata_single_mode_with_dc_pin(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A single-mode display with no explicit drawing gets a test card from final validation."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    config = validated_config(
        {
            "model": "ST7735",
            "dc_pin": 18,
        }
    )
    get_instance(config)
    meta = get_display_metadata(str(config["id"]))
    assert meta is not None
    assert meta.width == 128
    assert meta.height == 160
    assert meta.has_writer is True
    assert meta.has_hardware_rotation is True


def test_metadata_custom_dimensions(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A custom model picks up explicit dimensions."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    config = validated_config(
        {
            "model": "custom",
            "dc_pin": 18,
            "dimensions": {"width": 480, "height": 320},
            "init_sequence": [[0xA0, 0x01]],
        }
    )
    get_instance(config)
    meta = get_display_metadata(str(config["id"]))
    assert meta is not None
    assert meta.width == 480
    assert meta.height == 320
    # final validation auto-enables show_test_card
    assert meta.has_writer is True
    assert meta.has_hardware_rotation is True


def test_metadata_with_test_card_has_writer(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """When show_test_card is enabled, has_writer should be True."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
    )
    config = validated_config(
        {
            "model": "custom",
            "dc_pin": 18,
            "dimensions": {"width": 240, "height": 240},
            "init_sequence": [[0xA0, 0x01]],
            "show_test_card": True,
        }
    )
    get_instance(config)
    meta = get_display_metadata(str(config["id"]))
    assert meta is not None
    assert meta.has_writer is True


def test_metadata_no_swap_xy_not_full_hardware_rotation(
    set_core_config: SetCoreConfigCallable,
) -> None:
    """A model that disables swap_xy should report has_hardware_rotation=False."""
    set_core_config(
        PlatformFramework.ESP32_IDF,
        platform_data={KEY_BOARD: "esp32-s3-devkitc-1", KEY_VARIANT: VARIANT_ESP32S3},
    )
    # JC3248W535 has swap_xy=cv.UNDEFINED -> transforms={mirror_x, mirror_y} only
    config = validated_config({"model": "JC3248W535"})
    get_instance(config)
    meta = get_display_metadata(str(config["id"]))
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
    config_a = validated_config(
        {
            "id": "disp_a",
            "model": "custom",
            "dc_pin": 18,
            "dimensions": {"width": 320, "height": 240},
            "init_sequence": [[0xA0, 0x01]],
        }
    )
    config_b = validated_config(
        {
            "id": "disp_b",
            "model": "custom",
            "dc_pin": 19,
            "dimensions": {"width": 128, "height": 64},
            "init_sequence": [[0xA0, 0x01]],
        }
    )
    get_instance(config_a)
    get_instance(config_b)

    all_meta = get_all_display_metadata()
    # final validation auto-enables show_test_card for both
    assert all_meta["disp_a"] == DisplayMetaData(320, 240, True, True)
    assert all_meta["disp_b"] == DisplayMetaData(128, 64, True, True)


def test_metadata_via_code_generation_native(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
) -> None:
    """Full code generation for native.yaml should produce correct metadata."""
    generate_main(component_fixture_path("native.yaml"))
    all_meta = get_all_display_metadata()
    # native.yaml: model JC3636W518 -> 360x360, no writer, full hardware rotation
    assert len(all_meta) == 1
    meta = next(iter(all_meta.values()))
    assert meta == DisplayMetaData(
        width=360, height=360, has_writer=True, has_hardware_rotation=True
    )


def test_metadata_via_code_generation_lvgl(
    generate_main: Callable[[str | Path], str],
    component_fixture_path: Callable[[str], Path],
) -> None:
    """Full code generation for lvgl.yaml should produce correct metadata."""
    generate_main(component_fixture_path("lvgl.yaml"))
    all_meta = get_all_display_metadata()
    # lvgl.yaml: model ST7735 -> 128x160, no writer (lvgl draws directly), full hw rotation
    assert len(all_meta) == 1
    meta = next(iter(all_meta.values()))
    assert meta == DisplayMetaData(
        width=128, height=160, has_writer=False, has_hardware_rotation=True
    )
