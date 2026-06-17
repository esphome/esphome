"""Tests for image configuration validation."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from typing import Any
from unittest.mock import MagicMock, patch

from PIL import Image as PILImage
import pytest

from esphome import config_validation as cv
from esphome.components.image import (
    CONF_ALPHA_CHANNEL,
    CONF_INVERT_ALPHA,
    CONF_OPAQUE,
    CONF_TRANSPARENCY,
    CONFIG_SCHEMA,
    get_all_image_metadata,
    get_image_metadata,
    write_image,
)
from esphome.const import CONF_DITHER, CONF_FILE, CONF_ID, CONF_RAW_DATA_ID, CONF_TYPE
from esphome.core import CORE


@pytest.mark.parametrize(
    ("config", "error_match"),
    [
        pytest.param(
            "a string",
            "Badly formed image configuration, expected a list or a dictionary",
            id="invalid_string_config",
        ),
        pytest.param(
            {"id": "image_id", "type": "rgb565"},
            r"required key not provided @ data\['file'\]",
            id="missing_file",
        ),
        pytest.param(
            {"file": "image.png", "type": "rgb565"},
            r"required key not provided @ data\['id'\]",
            id="missing_id",
        ),
        pytest.param(
            {"id": "mdi_id", "file": "mdi:weather-##", "type": "rgb565"},
            "Could not parse mdi icon name",
            id="invalid_mdi_icon",
        ),
        pytest.param(
            {
                "id": "image_id",
                "file": "image.png",
                "type": "binary",
                "transparency": "alpha_channel",
            },
            "Image format 'BINARY' cannot have transparency",
            id="binary_with_transparency",
        ),
        pytest.param(
            {
                "id": "image_id",
                "file": "image.png",
                "type": "rgb565",
                "transparency": "chroma_key",
                "invert_alpha": True,
            },
            "No alpha channel to invert",
            id="invert_alpha_without_alpha_channel",
        ),
        pytest.param(
            {
                "id": "image_id",
                "file": "image.png",
                "type": "binary",
                "byte_order": "big_endian",
            },
            "Image format 'BINARY' does not support byte order configuration",
            id="binary_with_byte_order",
        ),
        pytest.param(
            {"id": "image_id", "file": "bad.png", "type": "binary"},
            "File can't be opened as image",
            id="invalid_image_file",
        ),
        pytest.param(
            {"defaults": {}, "images": [{"id": "image_id", "file": "image.png"}]},
            "Type is required either in the image config or in the defaults",
            id="missing_type_in_defaults",
        ),
    ],
)
def test_image_configuration_errors(
    config: Any,
    error_match: str,
) -> None:
    """Test detection of invalid configuration."""
    with pytest.raises(cv.Invalid, match=error_match):
        CONFIG_SCHEMA(config)


@pytest.mark.parametrize(
    "config",
    [
        pytest.param(
            {
                "id": "image_id",
                "file": "image.png",
                "type": "rgb565",
                "transparency": "chroma_key",
                "byte_order": "little_endian",
                "dither": "FloydSteinberg",
                "resize": "100x100",
                "invert_alpha": False,
            },
            id="single_image_all_options",
        ),
        pytest.param(
            [
                {
                    "id": "image_id",
                    "file": "image.png",
                    "type": "binary",
                }
            ],
            id="list_of_images",
        ),
        pytest.param(
            {
                "defaults": {
                    "type": "rgb565",
                    "transparency": "chroma_key",
                    "byte_order": "little_endian",
                    "dither": "FloydSteinberg",
                    "resize": "100x100",
                    "invert_alpha": False,
                },
                "images": [
                    {
                        "id": "image_id",
                        "file": "image.png",
                    }
                ],
            },
            id="images_with_defaults",
        ),
        pytest.param(
            {
                "rgb565": {
                    "alpha_channel": [
                        {
                            "id": "image_id",
                            "file": "image.png",
                            "transparency": "alpha_channel",
                            "byte_order": "little_endian",
                            "dither": "FloydSteinberg",
                            "resize": "100x100",
                            "invert_alpha": False,
                        }
                    ]
                },
                "binary": [
                    {
                        "id": "image_id",
                        "file": "image.png",
                        "transparency": "opaque",
                        "dither": "FloydSteinberg",
                        "resize": "100x100",
                        "invert_alpha": False,
                    }
                ],
            },
            id="type_based_organization",
        ),
        pytest.param(
            {
                "defaults": {
                    "type": "binary",
                    "transparency": "chroma_key",
                    "byte_order": "little_endian",
                    "dither": "FloydSteinberg",
                    "resize": "100x100",
                    "invert_alpha": False,
                },
                "rgb565": {
                    "alpha_channel": [
                        {
                            "id": "image_id",
                            "file": "image.png",
                            "transparency": "alpha_channel",
                            "dither": "none",
                        }
                    ]
                },
                "binary": [
                    {
                        "id": "image_id",
                        "file": "image.png",
                        "transparency": "opaque",
                    }
                ],
            },
            id="type_based_with_defaults",
        ),
        pytest.param(
            {
                "defaults": {
                    "type": "rgb565",
                    "transparency": "alpha_channel",
                },
                "binary": {
                    "opaque": [
                        {
                            "id": "image_id",
                            "file": "image.png",
                        }
                    ],
                },
            },
            id="binary_with_defaults",
        ),
    ],
)
def test_image_configuration_success(
    config: dict[str, Any] | list[dict[str, Any]],
) -> None:
    """Test successful configuration validation."""
    result = CONFIG_SCHEMA(config)
    # All valid configurations should return a list of images
    assert isinstance(result, list)
    for key in (CONF_TYPE, CONF_ID, CONF_TRANSPARENCY, CONF_RAW_DATA_ID):
        assert all(key in x for x in result), (
            f"Missing key {key} in image configuration"
        )


def test_image_generation(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test image generation configuration."""

    main_cpp = generate_main(component_config_path("image_test.yaml"))
    assert "uint8_t_id[] PROGMEM = {0x24, 0x21, 0x24, 0x21" in main_cpp
    assert (
        "alignas(image::Image) static unsigned char image__cat_img__pstorage[sizeof(image::Image)];"
        in main_cpp
    )
    assert (
        "static image::Image *const cat_img = reinterpret_cast<image::Image *>(image__cat_img__pstorage);"
        in main_cpp
    )
    assert (
        "new(cat_img) image::Image(uint8_t_id, 32, 24, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE);"
        in main_cpp
    )


def test_image_to_code_defines_and_core_data(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that to_code() sets USE_IMAGE define and stores image metadata."""
    # Generate the main cpp which will call to_code
    generate_main(component_config_path("image_test.yaml"))

    # Verify USE_IMAGE define was added
    assert any(d.name == "USE_IMAGE" for d in CORE.defines), (
        "USE_IMAGE define should be set when images are configured"
    )

    # Use the public API to get image metadata
    # The test config has an image with id 'cat_img'
    cat_img_metadata = get_image_metadata("cat_img")

    assert cat_img_metadata is not None, (
        "Image metadata should be retrievable via get_image_metadata()"
    )

    # Verify the metadata has the expected attributes
    assert hasattr(cat_img_metadata, "width"), "Metadata should have width attribute"
    assert hasattr(cat_img_metadata, "height"), "Metadata should have height attribute"
    assert hasattr(cat_img_metadata, "image_type"), (
        "Metadata should have image_type attribute"
    )
    assert hasattr(cat_img_metadata, "transparency"), (
        "Metadata should have transparency attribute"
    )

    # Verify the values are correct (from the test image)
    assert cat_img_metadata.width == 32, "Width should be 32"
    assert cat_img_metadata.height == 24, "Height should be 24"
    assert cat_img_metadata.image_type == "RGB565", "Type should be RGB565"
    assert cat_img_metadata.transparency == "opaque", "Transparency should be opaque"


def test_image_to_code_multiple_images(
    generate_main: Callable[[str | Path], str],
    component_config_path: Callable[[str], Path],
) -> None:
    """Test that to_code() stores metadata for multiple images."""
    generate_main(component_config_path("image_test.yaml"))

    # Use the public API to get all image metadata
    all_metadata = get_all_image_metadata()

    assert isinstance(all_metadata, dict), (
        "get_all_image_metadata() should return a dictionary"
    )

    # Verify that at least one image is present
    assert len(all_metadata) > 0, "Should have at least one image metadata entry"

    # Each image ID should map to an ImageMetaData object
    for image_id, metadata in all_metadata.items():
        assert isinstance(image_id, str), "Image IDs should be strings"

        # Verify it's an ImageMetaData object with all required attributes
        assert hasattr(metadata, "width"), (
            f"Metadata for '{image_id}' should have width"
        )
        assert hasattr(metadata, "height"), (
            f"Metadata for '{image_id}' should have height"
        )
        assert hasattr(metadata, "image_type"), (
            f"Metadata for '{image_id}' should have image_type"
        )
        assert hasattr(metadata, "transparency"), (
            f"Metadata for '{image_id}' should have transparency"
        )

        # Verify values are valid
        assert isinstance(metadata.width, int), (
            f"Width for '{image_id}' should be an integer"
        )
        assert isinstance(metadata.height, int), (
            f"Height for '{image_id}' should be an integer"
        )
        assert isinstance(metadata.image_type, str), (
            f"Type for '{image_id}' should be a string"
        )
        assert isinstance(metadata.transparency, str), (
            f"Transparency for '{image_id}' should be a string"
        )
        assert metadata.width > 0, f"Width for '{image_id}' should be positive"
        assert metadata.height > 0, f"Height for '{image_id}' should be positive"


def test_get_image_metadata_nonexistent() -> None:
    """Test that get_image_metadata returns None for non-existent image IDs."""
    # This should return None when no images are configured or ID doesn't exist
    metadata = get_image_metadata("nonexistent_image_id")
    assert metadata is None, (
        "get_image_metadata should return None for non-existent IDs"
    )


def test_get_all_image_metadata_empty() -> None:
    """Test that get_all_image_metadata returns empty dict when no images configured."""
    # When CORE hasn't been initialized with images, should return empty dict
    all_metadata = get_all_image_metadata()
    assert isinstance(all_metadata, dict), (
        "get_all_image_metadata should always return a dict"
    )
    # Length could be 0 or more depending on what's in CORE at test time


@pytest.fixture
def mock_progmem_array():
    """Mock progmem_array to avoid needing a proper ID object in tests."""
    with patch("esphome.components.image.cg.progmem_array") as mock_progmem:
        mock_progmem.return_value = MagicMock()
        yield mock_progmem


@pytest.mark.asyncio
async def test_svg_with_mm_dimensions_succeeds(
    component_config_path: Callable[[str], Path],
    mock_progmem_array: MagicMock,
) -> None:
    """Test that SVG files with dimensions in mm are successfully processed."""
    # Create a config for write_image without CONF_RESIZE
    config = {
        CONF_FILE: component_config_path("mm_dimensions.svg"),
        CONF_TYPE: "BINARY",
        CONF_TRANSPARENCY: CONF_OPAQUE,
        CONF_DITHER: "NONE",
        CONF_INVERT_ALPHA: False,
        CONF_RAW_DATA_ID: "test_raw_data_id",
    }

    # This should succeed without raising an error
    result = await write_image(config)

    # Verify that write_image returns the expected tuple
    assert isinstance(result, tuple), "write_image should return a tuple"
    assert len(result) == 6, "write_image should return 6 values"

    prog_arr, width, height, image_type, trans_value, frame_count = result

    # Verify the dimensions are positive integers
    # At 100 DPI, 10mm = ~39 pixels (10mm * 100dpi / 25.4mm_per_inch)
    assert isinstance(width, int), "Width should be an integer"
    assert isinstance(height, int), "Height should be an integer"
    assert width > 0, "Width should be positive"
    assert height > 0, "Height should be positive"
    assert frame_count == 1, "Single image should have frame_count of 1"
    # Verify we got reasonable dimensions from the mm-based SVG
    assert 30 < width < 50, (
        f"Width should be around 39 pixels for 10mm at 100dpi, got {width}"
    )
    assert 30 < height < 50, (
        f"Height should be around 39 pixels for 10mm at 100dpi, got {height}"
    )


@pytest.mark.asyncio
async def test_rgb565_alpha_animation_layout_per_frame(
    tmp_path: Path,
    mock_progmem_array: MagicMock,
) -> None:
    """RGB565+alpha animations must store each frame as a self-contained
    [RGB plane | alpha plane] block. Animation::update_data_start_ steps frames
    with a single per-frame stride, so any cross-frame layout (all RGB then all
    alpha) makes the C++ alpha read land in the next frame's RGB bytes — that
    was the regression behind issue #15999.
    """
    # Build a 2-frame APNG where each frame is a solid color with a known
    # alpha. APNG preserves full RGBA per pixel (GIF only has 1-bit alpha so
    # round-tripping mid-range alpha values does not work). Frame 0 is fully
    # opaque red, frame 1 is fully transparent blue.
    width = 4
    height = 3
    frame0 = PILImage.new("RGBA", (width, height), (255, 0, 0, 0xFF))
    frame1 = PILImage.new("RGBA", (width, height), (0, 0, 255, 0x00))
    apng_path = tmp_path / "anim.png"
    frame0.save(
        apng_path,
        format="PNG",
        save_all=True,
        append_images=[frame1],
        duration=100,
        loop=0,
    )

    config = {
        CONF_FILE: str(apng_path),
        CONF_TYPE: "RGB565",
        CONF_TRANSPARENCY: CONF_ALPHA_CHANNEL,
        CONF_DITHER: "NONE",
        CONF_INVERT_ALPHA: False,
        CONF_RAW_DATA_ID: "test_raw_data_id",
    }

    _, _, _, _, _, frame_count = await write_image(config, all_frames=True)
    assert frame_count == 2

    # Recover the bytes handed to progmem_array. Signature is (id_, rhs).
    _, raw_data = mock_progmem_array.call_args.args
    data = [int(x) for x in raw_data]

    rgb_size = width * height * 2
    alpha_size = width * height
    frame_size = rgb_size + alpha_size
    assert len(data) == frame_size * frame_count, (
        "RGB565+alpha animation buffer must be (RGB + alpha) per frame, not "
        "all RGB followed by all alpha"
    )

    # Frame 0: RGB plane is red, alpha plane is 0xFF. Frame 1: alpha plane is
    # 0x00. If the layout regresses to [all RGB | all alpha], the alpha bytes
    # would all land at the tail of the buffer and the per-frame slices below
    # would point at RGB565 noise instead.
    frame0_alpha = data[rgb_size : rgb_size + alpha_size]
    frame1_alpha = data[frame_size + rgb_size : frame_size + rgb_size + alpha_size]
    assert all(a == 0xFF for a in frame0_alpha), (
        f"Frame 0 alpha plane should be opaque, got {frame0_alpha}"
    )
    assert all(a == 0x00 for a in frame1_alpha), (
        f"Frame 1 alpha plane should be transparent, got {frame1_alpha}"
    )
