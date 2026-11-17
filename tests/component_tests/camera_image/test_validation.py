"""Tests for camera_image configuration validation."""

from collections.abc import Callable

import pytest

from esphome import config_validation as cv
from esphome.components import esp32_camera
from esphome.components.camera_image import image as camera_image
from esphome.config import Config
from esphome.const import CONF_ID, PlatformFramework
from esphome.core import ID
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


def _full_config(**camera_options: object) -> Config:
    """A full config declaring the esp32_camera that BASE refers to."""
    camera_id = ID("test_camera", is_declaration=True)
    camera = {
        CONF_ID: camera_id,
        esp32_camera.CONF_PIXEL_FORMAT: camera_options.get("pixel_format", "JPEG"),
        esp32_camera.CONF_JPEG_QUALITY: camera_options.get("jpeg_quality", 10),
        esp32_camera.CONF_RESOLUTION: str(camera_options.get("resolution", "640X480")),
    }
    full = Config()
    full[esp32_camera.DOMAIN] = camera
    full.declare_ids.append((camera_id, [esp32_camera.DOMAIN, CONF_ID]))
    return full


def _validate_final(
    set_core_config: Callable[..., None],
    extra: ConfigType | None = None,
    **camera_options: object,
) -> ConfigType:
    """Validate the platform options, then the checks that need the camera config."""
    config = _validate(set_core_config, extra)
    set_core_config(
        PlatformFramework.ESP32_IDF, full_config=_full_config(**camera_options)
    )
    return camera_image.FINAL_VALIDATE_SCHEMA(config)


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
    _validate_final(set_core_config, {"resize": "320x240", "crop": crop})


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
    with pytest.raises(cv.Invalid, match="does not fit inside the 320x240 frame"):
        _validate_final(set_core_config, {"resize": "320x240", "crop": crop})


def test_crop_past_decoded_frame_when_aspect_ratios_differ(
    set_core_config: Callable[..., None],
) -> None:
    # A 640x480 frame resized into 320x320 decodes to 320x240, so a 300 pixel
    # tall crop passes the resize bounds but can never fit the decoded frame.
    crop = {"x": 0, "y": 0, "width": 100, "height": 300}
    with pytest.raises(
        cv.Invalid,
        match="does not fit inside the 320x240 frame decoded from the 640x480 "
        "camera with resize 320x320",
    ):
        _validate_final(set_core_config, {"resize": "320x320", "crop": crop})


def test_crop_at_decoded_height_is_accepted(
    set_core_config: Callable[..., None],
) -> None:
    crop = {"x": 0, "y": 0, "width": 100, "height": 240}
    _validate_final(set_core_config, {"resize": "320x320", "crop": crop})


def test_decoded_size_without_resize_is_the_camera_resolution(
    set_core_config: Callable[..., None],
) -> None:
    config = _validate_final(set_core_config, resolution="640X480")
    assert config[camera_image._KEY_DECODED_SIZE] == (640, 480)


def test_decoded_size_with_resize_is_the_fitted_frame(
    set_core_config: Callable[..., None],
) -> None:
    # 640x480 fitted uniformly into 320x320 leaves the 320x240 frame that is
    # published and that the metadata and rotation buffer have to use.
    config = _validate_final(
        set_core_config, {"resize": "320x320"}, resolution="640X480"
    )
    assert config[camera_image._KEY_DECODED_SIZE] == (320, 240)


def test_decoded_size_keeps_the_aspect_ratio_when_the_box_matches(
    set_core_config: Callable[..., None],
) -> None:
    config = _validate_final(
        set_core_config, {"resize": "320x240"}, resolution="640X480"
    )
    assert config[camera_image._KEY_DECODED_SIZE] == (320, 240)


def test_crop_with_alpha_channel_is_rejected(
    set_core_config: Callable[..., None],
) -> None:
    crop = {"x": 0, "y": 0, "width": 10, "height": 10}
    with pytest.raises(cv.Invalid, match="alpha_channel"):
        _validate(set_core_config, {"crop": crop, "transparency": "alpha_channel"})


def test_rotation_without_resize_or_crop_is_accepted(
    set_core_config: Callable[..., None],
) -> None:
    # The decoded size comes from the camera resolution, which is enough to size
    # the rotated output and its scratch buffer.
    config = _validate_final(set_core_config, {"rotation": 90})
    assert config["rotation"] == 90
    assert config[camera_image._KEY_DECODED_SIZE] == (640, 480)


def test_rotation_with_double_buffer_is_rejected(
    set_core_config: Callable[..., None],
) -> None:
    with pytest.raises(cv.Invalid, match="double_buffer"):
        _validate(set_core_config, {"rotation": 90, "double_buffer": True})


def test_raw_camera_is_rejected(set_core_config: Callable[..., None]) -> None:
    # Without jpeg_quality the camera sends raw pixels, which can never decode.
    with pytest.raises(cv.Invalid, match="delivers raw frames"):
        _validate_final(set_core_config, pixel_format="RGB565", jpeg_quality=0)


def test_raw_camera_with_jpeg_conversion_is_accepted(
    set_core_config: Callable[..., None],
) -> None:
    _validate_final(set_core_config, pixel_format="RGB565", jpeg_quality=10)


def test_jpeg_camera_is_accepted(set_core_config: Callable[..., None]) -> None:
    _validate_final(set_core_config)


def test_crop_past_camera_resolution_is_rejected(
    set_core_config: Callable[..., None],
) -> None:
    crop = {"x": 300, "y": 0, "width": 100, "height": 100}
    with pytest.raises(
        cv.Invalid, match="does not fit inside the 320x240 camera resolution"
    ):
        _validate_final(set_core_config, {"crop": crop}, resolution="320X240")


def test_crop_within_camera_resolution_is_accepted(
    set_core_config: Callable[..., None],
) -> None:
    crop = {"x": 10, "y": 10, "width": 100, "height": 100}
    _validate_final(set_core_config, {"crop": crop}, resolution="320X240")


def test_crop_checked_against_resolution_alias(
    set_core_config: Callable[..., None],
) -> None:
    # QVGA is the alias for 320x240.
    crop = {"x": 300, "y": 0, "width": 100, "height": 100}
    with pytest.raises(
        cv.Invalid, match="does not fit inside the 320x240 camera resolution"
    ):
        _validate_final(set_core_config, {"crop": crop}, resolution="QVGA")
