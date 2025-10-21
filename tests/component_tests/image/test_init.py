"""Tests for image configuration validation."""

from __future__ import annotations

from collections.abc import Callable
from pathlib import Path
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components.image import CONF_TRANSPARENCY, CONFIG_SCHEMA
from esphome.const import CONF_ID, CONF_RAW_DATA_ID, CONF_TYPE


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
        "cat_img = new image::Image(uint8_t_id, 32, 24, image::IMAGE_TYPE_RGB565, image::TRANSPARENCY_OPAQUE);"
        in main_cpp
    )
