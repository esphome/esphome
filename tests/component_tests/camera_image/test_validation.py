"""Tests for camera_image configuration validation."""

from collections.abc import Callable

import pytest

from esphome import config_validation as cv
from esphome.components.camera_image import image as camera_image
from esphome.const import PlatformFramework
from esphome.types import ConfigType

BASE: ConfigType = {
    "id": "test_camera_image",
    "camera_id": "test_camera",
    "format": "JPEG",
    "type": "RGB565",
}


def _validate(
    set_core_config: Callable[..., None], extra: ConfigType | None = None
) -> ConfigType:
    set_core_config(PlatformFramework.ESP32_IDF)
    config = dict(BASE)
    config.update(extra or {})
    return camera_image.CONFIG_SCHEMA(config)


def test_defaults(set_core_config: Callable[..., None]) -> None:
    config = _validate(set_core_config)
    assert config["format"] == "JPEG"
    assert config["rotation"] == 0
    assert config["transparency"] == "opaque"


@pytest.mark.parametrize("image_format", ["JPG", "jpg", "jpeg"])
def test_jpeg_alias_is_accepted(
    set_core_config: Callable[..., None], image_format: str
) -> None:
    config = _validate(set_core_config, {"format": image_format})
    assert config["format"] in ("JPEG", "JPG")


@pytest.mark.parametrize("image_format", ["BMP", "PNG", "QOI"])
def test_non_jpeg_format_is_rejected(
    set_core_config: Callable[..., None], image_format: str
) -> None:
    # A camera only produces JPEG frames, so another format could never decode.
    with pytest.raises(cv.Invalid, match=r"data\['format'\]"):
        _validate(set_core_config, {"format": image_format})


def test_crop_within_resize_is_accepted(
    set_core_config: Callable[..., None],
) -> None:
    crop = {"x": 100, "y": 50, "width": 220, "height": 190}
    config = _validate(set_core_config, {"resize": "320x240", "crop": crop})
    assert config["crop"]["width"] == 220


@pytest.mark.parametrize(
    "crop",
    [
        {"x": 200, "y": 0, "width": 200, "height": 100},
        {"x": 0, "y": 200, "width": 100, "height": 200},
    ],
)
def test_crop_past_resize_is_rejected(
    set_core_config: Callable[..., None], crop: dict[str, int]
) -> None:
    with pytest.raises(cv.Invalid, match="does not fit inside resize 320x240"):
        _validate(set_core_config, {"resize": "320x240", "crop": crop})


def test_crop_with_alpha_channel_is_rejected(
    set_core_config: Callable[..., None],
) -> None:
    crop = {"x": 0, "y": 0, "width": 10, "height": 10}
    with pytest.raises(cv.Invalid, match="alpha_channel"):
        _validate(set_core_config, {"crop": crop, "transparency": "alpha_channel"})


def test_rotation_with_double_buffer_is_rejected(
    set_core_config: Callable[..., None],
) -> None:
    with pytest.raises(cv.Invalid, match="double_buffer"):
        _validate(set_core_config, {"rotation": 90, "double_buffer": True})
