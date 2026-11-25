"""Tests for the frame, decoder and crop checks of ssocr_reader's final validation."""

from collections.abc import Callable
from typing import Any

import pytest

from esphome import config_validation as cv
from esphome.components import camera_decoder, esp32_camera, ssocr_reader
from esphome.const import CONF_RESOLUTION, CONF_ROTATION
from esphome.core import ID
from esphome.types import ConfigType

CAMERA_ID = ID("cam")
DECODER_ID = ID("dec")


def _config(**crop: int) -> ConfigType:
    return {
        ssocr_reader.CONF_CAMERA_ID: CAMERA_ID,
        ssocr_reader.CONF_DECODER_ID: DECODER_ID,
        ssocr_reader.CONF_CROP_X: crop.get("x", 0),
        ssocr_reader.CONF_CROP_Y: crop.get("y", 0),
        ssocr_reader.CONF_CROP_W: crop.get("w", 100),
        ssocr_reader.CONF_CROP_H: crop.get("h", 40),
    }


@pytest.fixture
def declared(monkeypatch: pytest.MonkeyPatch) -> Callable[..., None]:
    """Make the camera and decoder configs the final validation looks up settable."""

    def declare(
        pixel_format: str = "JPEG",
        jpeg_quality: int = 0,
        output_type: str = "PIXEL_FORMAT_GRAYSCALE",
        rotation: int = 0,
    ) -> None:
        configs: dict[ID, dict[str, Any]] = {
            CAMERA_ID: {
                CONF_RESOLUTION: "640X480",
                esp32_camera.CONF_PIXEL_FORMAT: pixel_format,
                esp32_camera.CONF_JPEG_QUALITY: jpeg_quality,
            },
            DECODER_ID: {
                camera_decoder.CONF_OUTPUT_TYPE: output_type,
                CONF_ROTATION: rotation,
            },
        }
        monkeypatch.setattr(
            ssocr_reader.fv, "get_declaring_config", lambda id_: configs[id_]
        )

    return declare


def test_accepts_a_crop_inside_the_frame(declared: Callable[..., None]) -> None:
    declared()

    config = ssocr_reader._final_validate(_config(x=540, y=440))

    assert config[ssocr_reader._KEY_CAMERA_FRAME] == (640, 480)


def test_accepts_a_converted_jpeg_frame(declared: Callable[..., None]) -> None:
    declared(pixel_format="RGB565", jpeg_quality=12)

    ssocr_reader._final_validate(_config())


@pytest.mark.parametrize(
    ("camera", "crop", "message"),
    [
        pytest.param(
            {"pixel_format": "RGB565", "jpeg_quality": 0},
            {},
            "needs JPEG frames",
            id="camera-without-jpeg",
        ),
        pytest.param(
            {"output_type": "PIXEL_FORMAT_RGB565"},
            {},
            "GRAYSCALE or RGB888",
            id="unsupported-decoder-output",
        ),
        pytest.param({}, {"x": 600, "w": 100}, "does not fit", id="crop-outside"),
    ],
)
def test_rejects_an_invalid_setup(
    declared: Callable[..., None],
    camera: dict[str, Any],
    crop: dict[str, int],
    message: str,
) -> None:
    declared(**camera)

    with pytest.raises(cv.Invalid, match=message):
        ssocr_reader._final_validate(_config(**crop))


def test_a_quarter_turn_swaps_the_frame_dimensions(
    declared: Callable[..., None],
) -> None:
    declared(rotation=90)

    assert ssocr_reader._final_validate(_config(w=40, h=600))[
        ssocr_reader._KEY_CAMERA_FRAME
    ] == (480, 640)
    with pytest.raises(cv.Invalid, match="does not fit"):
        ssocr_reader._final_validate(_config(w=600, h=40))
