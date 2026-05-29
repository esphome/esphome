"""Tests for padding, offset calculation, and SPI mode configuration in mipi_spi."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path

import pytest

from esphome.components.esp32 import (
    KEY_BOARD,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S3,
)
from esphome.components.mipi_spi.display import (
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    MODELS,
    get_instance,
)
from esphome.components.spi import CONF_SPI_MODE, TYPE_OCTAL, TYPE_QUAD, TYPE_SINGLE
from esphome.const import CONF_CS_PIN, CONF_DC_PIN, PlatformFramework
from esphome.types import ConfigType
from tests.component_tests.types import SetCoreConfigCallable


def validated_config(config: ConfigType) -> ConfigType:
    """Run schema + final validation and return the validated config."""
    config = CONFIG_SCHEMA(config)
    FINAL_VALIDATE_SCHEMA(config)
    return config


class TestSPIModeCalculation:
    """Test default SPI mode calculation logic."""

    @pytest.mark.parametrize(
        ("bus_mode", "cs_pin", "expected_mode"),
        [
            pytest.param(
                TYPE_OCTAL,
                None,
                "MODE3",
                id="octal_bus_no_cs",
            ),
            pytest.param(
                TYPE_OCTAL,
                14,
                "MODE3",
                id="octal_bus_with_cs",
            ),
            pytest.param(
                TYPE_SINGLE,
                None,
                "MODE3",
                id="single_bus_no_cs",
            ),
            pytest.param(
                TYPE_SINGLE,
                14,
                "MODE0",
                id="single_bus_with_cs",
            ),
            pytest.param(
                TYPE_QUAD,
                None,
                "MODE0",
                id="quad_bus_no_cs",
            ),
            pytest.param(
                TYPE_QUAD,
                14,
                "MODE0",
                id="quad_bus_with_cs",
            ),
        ],
    )
    def test_default_spi_mode_calculation(
        self,
        bus_mode: str,
        cs_pin: int | None,
        expected_mode: str,
        set_core_config: SetCoreConfigCallable,
    ) -> None:
        """Test that SPI mode is correctly calculated based on bus mode and CS pin."""
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={
                KEY_BOARD: "esp32-s3-devkitc-1",
                KEY_VARIANT: VARIANT_ESP32S3,
            },
        )

        config: ConfigType = {
            "model": "custom",
            "dimensions": {"width": 320, "height": 240},
            "init_sequence": [[0xA0, 0x01]],
            "bus_mode": bus_mode,
        }

        # Add dc_pin for modes that require it (single and octal)
        # quad mode does not allow dc_pin
        if bus_mode != TYPE_QUAD:
            config[CONF_DC_PIN] = 11

        # Add CS pin if specified
        if cs_pin is not None:
            config[CONF_CS_PIN] = cs_pin

        validated = validated_config(config)
        # The validated config should have the correct SPI mode set by model_schema
        assert validated.get(CONF_SPI_MODE) == expected_mode

    def test_explicit_spi_mode_overrides_default(
        self,
        set_core_config: SetCoreConfigCallable,
    ) -> None:
        """Test that an explicitly configured SPI mode is not overridden."""
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={
                KEY_BOARD: "esp32-s3-devkitc-1",
                KEY_VARIANT: VARIANT_ESP32S3,
            },
        )

        # For octal bus, default is MODE3, but we specify MODE0
        config = validated_config(
            {
                "model": "custom",
                "dc_pin": 11,  # Required for octal mode
                "dimensions": {"width": 320, "height": 240},
                "init_sequence": [[0xA0, 0x01]],
                "bus_mode": TYPE_OCTAL,
                "spi_mode": "MODE0",  # Explicitly set
            }
        )

        assert config[CONF_SPI_MODE] == "MODE0"


class TestModelWithPaddingDimensions:
    """Test that padding dimensions are correctly returned by models."""

    def test_model_get_dimensions_returns_six_values(
        self,
        set_core_config: SetCoreConfigCallable,
    ) -> None:
        """Test that get_dimensions() returns 6 values including padding."""
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={
                KEY_BOARD: "esp32-s3-devkitc-1",
                KEY_VARIANT: VARIANT_ESP32S3,
            },
        )

        # Test with a real model
        model = MODELS["ST7735"]
        config = {"model": "ST7735", "dc_pin": 18}

        # Call get_dimensions - should return 6 values (width, height, offset_x, offset_y, pad_width, pad_height)
        dimensions = model.get_dimensions(config)
        assert len(dimensions) == 6
        assert all(isinstance(v, int) for v in dimensions)

    def test_custom_model_padding_values(
        self,
        set_core_config: SetCoreConfigCallable,
    ) -> None:
        """Test padding values for a custom model with explicit offset."""
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
        )

        config = validated_config(
            {
                "model": "custom",
                "dc_pin": 18,
                "dimensions": {
                    "width": 240,
                    "height": 320,
                    "offset_width": 20,
                    "offset_height": 10,
                },
                "init_sequence": [[0xA0, 0x01]],
            }
        )

        # For custom models, the model is created dynamically from the config
        # We can verify the config has the right dimensions
        assert config["dimensions"]["width"] == 240
        assert config["dimensions"]["height"] == 320
        assert config["dimensions"]["offset_width"] == 20
        assert config["dimensions"]["offset_height"] == 10
        # Padding is not stored in config for custom models (defaults to 0)
        assert config["dimensions"].get("offset_width_pad", 0) == 0
        assert config["dimensions"].get("offset_height_pad", 0) == 0


class TestNewModelVariants:
    """Test new model variants added in this change."""

    def test_m5core2_with_native_dimensions(
        self,
        set_core_config: SetCoreConfigCallable,
    ) -> None:
        """Test M5CORE2 variant with reset native_width and native_height."""
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={
                KEY_BOARD: "esp32-s3-devkitc-1",
                KEY_VARIANT: VARIANT_ESP32S3,
            },
        )

        # M5CORE2 should validate successfully
        config = validated_config({"model": "M5CORE2"})
        assert config is not None

        # Verify the model has correct dimensions
        model = MODELS["M5CORE2"]
        dimensions = model.get_dimensions(config)
        width, height, _, _, _, _ = dimensions
        assert width == 320
        assert height == 240

    def test_geekmagic_smalltv_variant(
        self,
        set_core_config: SetCoreConfigCallable,
    ) -> None:
        """Test GEEKMAGIC-SMALLTV variant of ST7789V."""
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
        )

        # GEEKMAGIC-SMALLTV should validate successfully
        config = validated_config({"model": "GEEKMAGIC-SMALLTV"})
        assert config is not None

        # Verify it's a variant of ST7789V with expected dimensions
        model = MODELS["GEEKMAGIC-SMALLTV"]
        dimensions = model.get_dimensions(config)
        width, height, offset_x, offset_y, _, _ = dimensions
        assert width == 240
        assert height == 240
        assert offset_x == 0
        assert offset_y == 0

    def test_all_predefined_models_with_new_get_dimensions_signature(
        self,
        set_core_config: SetCoreConfigCallable,
    ) -> None:
        """Verify all predefined models work with new 6-value get_dimensions()."""
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={
                KEY_BOARD: "esp32-s3-devkitc-1",
                KEY_VARIANT: VARIANT_ESP32S3,
            },
        )

        for name, model in MODELS.items():
            # Skip custom model
            if name == "custom":
                continue

            config = {"model": name}

            # Try to get dimensions - should return 6 values for all models
            dimensions = model.get_dimensions(config)
            assert len(dimensions) == 6, (
                f"Model {name} should return 6 dimensions, got {len(dimensions)}"
            )


class TestTemplateParameterPassing:
    """Test that padding parameters are correctly passed to C++ templates."""

    def test_instance_creation_with_padding(
        self,
        generate_main: Callable[[str | Path], str],
        component_fixture_path: Callable[[str], Path],
    ) -> None:
        """Test that get_instance() correctly passes padding parameters to template."""
        main_cpp = generate_main(component_fixture_path("native.yaml"))

        # native.yaml uses JC3636W518 which should have 8 template parameters for MipiSpiBuffer
        # (BUFFERTYPE, BUFFERPIXEL, IS_BIG_ENDIAN, DISPLAYPIXEL, BUS_TYPE,
        #  WIDTH, HEIGHT, OFFSET_WIDTH, OFFSET_HEIGHT, PAD_WIDTH, PAD_HEIGHT, MADCTL, HAS_HARDWARE_ROTATION,
        #  FRACTION, ROUNDING)
        # The instantiation should include padding values (0, 0 for default)
        assert (
            "mipi_spi::MipiSpiBuffer<uint16_t, mipi_spi::PIXEL_MODE_16, true, mipi_spi::PIXEL_MODE_16, mipi_spi::BUS_TYPE_QUAD, 360, 360, 0, 1, 0, 0, 0, true, 1, 1>()"
            in main_cpp
        ), (
            "Padding parameters (0, 0) should be in the MipiSpiBuffer template instantiation"
        )

    def test_single_mode_with_offset_padding(
        self,
        set_core_config: SetCoreConfigCallable,
    ) -> None:
        """Test that single-mode display with custom offset works with padding."""
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
        )

        config = validated_config(
            {
                "model": "custom",
                "dc_pin": 18,
                "dimensions": {
                    "width": 240,
                    "height": 320,
                    "offset_width": 40,
                    "offset_height": 20,
                },
                "init_sequence": [[0xA0, 0x01]],
                "buffer_size": 0.25,
            }
        )

        # Should not raise any errors
        instance = get_instance(config)
        assert instance is not None


class TestUserConfiguredPadding:
    """Test that pad_width and pad_height can be configured in user dimensions."""

    def test_explicit_pad_width_and_height_in_dimensions(
        self,
        set_core_config: SetCoreConfigCallable,
    ) -> None:
        """Test that pad_width and pad_height can be explicitly set in dimensions."""
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
        )

        config = validated_config(
            {
                "model": "custom",
                "dc_pin": 18,
                "dimensions": {
                    "width": 240,
                    "height": 320,
                    "offset_width": 40,
                    "offset_height": 20,
                    "pad_width": 80,
                    "pad_height": 40,
                },
                "init_sequence": [[0xA0, 0x01]],
                "buffer_size": 0.25,
            }
        )

        # Config should validate successfully with padding dimensions
        assert config is not None
        assert config["dimensions"]["pad_width"] == 80
        assert config["dimensions"]["pad_height"] == 40

    def test_padding_for_native_dimension_calculation(
        self,
        set_core_config: SetCoreConfigCallable,
    ) -> None:
        """Test that explicit padding allows native dimensions to be calculated."""
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
        )

        # A controller that has 320x320 total pixels with:
        # - 240x320 active display area
        # - offset_width=40, offset_height=20
        # - pad_width=40 (remaining pixels on right), pad_height=60 (remaining pixels on bottom)
        config = validated_config(
            {
                "model": "custom",
                "dc_pin": 18,
                "dimensions": {
                    "width": 240,  # Active display width
                    "height": 320,  # Active display height
                    "offset_width": 40,
                    "offset_height": 0,
                    "pad_width": 40,  # Pixels after width+offset
                    "pad_height": 0,  # Pixels after height+offset
                },
                "init_sequence": [[0xA0, 0x01]],
                "buffer_size": 0.25,
            }
        )

        # Get instance should work and correctly calculate native dimensions
        instance = get_instance(config)
        assert instance is not None

    def test_padding_without_offset(
        self,
        set_core_config: SetCoreConfigCallable,
    ) -> None:
        """Test padding can be used without offset for controllers with top-left-aligned displays."""
        set_core_config(
            PlatformFramework.ESP32_IDF,
            platform_data={KEY_BOARD: "esp32dev", KEY_VARIANT: VARIANT_ESP32},
        )

        # A display with no offset but padding on right and bottom
        config = validated_config(
            {
                "model": "custom",
                "dc_pin": 18,
                "dimensions": {
                    "width": 240,
                    "height": 240,
                    "offset_width": 0,
                    "offset_height": 0,
                    "pad_width": 0,
                    "pad_height": 16,
                },
                "init_sequence": [[0xA0, 0x01]],
                "buffer_size": 0.25,
            }
        )

        assert config is not None
        assert config["dimensions"]["width"] == 240
        assert config["dimensions"]["height"] == 240
        assert config["dimensions"]["pad_height"] == 16
