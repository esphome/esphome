"""Reads seven-segment displays out of esp32_camera frames."""

import esphome.codegen as cg
from esphome.components import camera, camera_decoder, esp32_camera
import esphome.config_validation as cv
from esphome.const import CONF_DEBUG, CONF_ID, CONF_RESOLUTION, CONF_ROTATION
import esphome.final_validate as fv
from esphome.types import ConfigType

CODEOWNERS = ["@nliaudat"]
AUTO_LOAD = ["seven_segment_ocr", "sensor"]
DEPENDENCIES = ["esp32_camera", "camera_decoder"]
DOMAIN = "ssocr_reader"

CONF_CAMERA_ID = "camera_id"
CONF_DECODER_ID = "decoder_id"
CONF_CROP_X = "crop_x"
CONF_CROP_Y = "crop_y"
CONF_CROP_W = "crop_w"
CONF_CROP_H = "crop_h"
CONF_THRESHOLD_LEVEL = "threshold_level"
CONF_DIGIT_COUNT = "digit_count"

ssocr_reader_ns = cg.esphome_ns.namespace("ssocr_reader")
SSOCRReader = ssocr_reader_ns.class_("SSOCRReader", cg.PollingComponent)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SSOCRReader),
        cv.Required(CONF_CAMERA_ID): cv.use_id(esp32_camera.ESP32Camera),
        cv.Required(CONF_DECODER_ID): cv.use_id(camera.Decoder),
        cv.Optional(CONF_CROP_X, default=0): cv.int_range(min=0),
        cv.Optional(CONF_CROP_Y, default=0): cv.int_range(min=0),
        cv.Required(CONF_CROP_W): cv.positive_not_null_int,
        cv.Required(CONF_CROP_H): cv.positive_not_null_int,
        cv.Optional(CONF_THRESHOLD_LEVEL, default=128): cv.int_range(min=0, max=255),
        cv.Optional(CONF_DIGIT_COUNT, default=6): cv.int_range(min=1, max=9),
        cv.Optional(CONF_DEBUG, default=False): cv.boolean,
    }
).extend(cv.polling_component_schema("60s"))

# Frame size resolved in final_validate and read back by to_code, so codegen
# never has to guess it from substitutions.
_KEY_CAMERA_FRAME = "_ssocr_camera_frame"


def _enum_name(value: object) -> str:
    """Return the C++ enumerator name behind a validated ``cv.enum`` value."""
    return str(getattr(value, "enum_value", value)).rsplit("::", 1)[-1]


def _resolution_dimensions(resolution: object) -> tuple[int, int]:
    """Map an esp32_camera resolution to its pixel dimensions."""
    name = _enum_name(resolution).upper().removeprefix("ESP32_CAMERA_SIZE_")
    parts = name.split("X")
    if len(parts) == 2 and parts[0].isdigit() and parts[1].isdigit():
        return int(parts[0]), int(parts[1])
    raise cv.Invalid(f"Unknown camera resolution {resolution}")


def _final_validate(config: ConfigType) -> ConfigType:
    """Resolve the decoded frame size and check it against the crop and the decoder."""
    camera_config = fv.get_declaring_config(config[CONF_CAMERA_ID])
    width, height = _resolution_dimensions(camera_config[CONF_RESOLUTION])

    pixel_format = camera_config[esp32_camera.CONF_PIXEL_FORMAT]
    # A non-zero jpeg_quality makes esp32_camera convert the frame to JPEG, so
    # the listener receives JPEG even when pixel_format is not JPEG.
    jpeg_quality = camera_config[esp32_camera.CONF_JPEG_QUALITY]
    delivered = "JPEG" if pixel_format == "JPEG" or jpeg_quality != 0 else pixel_format
    if delivered != "JPEG":
        raise cv.Invalid(
            f"ssocr_reader needs JPEG frames, but the referenced camera "
            f"delivers {delivered}. Set pixel_format: JPEG, or set a jpeg_quality "
            f"so the camera converts the frame to JPEG.",
            path=[CONF_CAMERA_ID],
        )

    decoder_config = fv.get_declaring_config(config[CONF_DECODER_ID])
    output_format = _enum_name(decoder_config[camera_decoder.CONF_OUTPUT_TYPE])
    if output_format not in ("PIXEL_FORMAT_GRAYSCALE", "PIXEL_FORMAT_RGB888"):
        raise cv.Invalid(
            "ssocr_reader needs the decoder's GRAYSCALE or RGB888 output. Set "
            "output_type on the referenced camera_decoder.",
            path=[CONF_DECODER_ID],
        )

    # A quarter turn swaps the decoded frame dimensions that the crop is measured against.
    if decoder_config[CONF_ROTATION] in (90, 270):
        width, height = height, width

    if (
        config[CONF_CROP_X] + config[CONF_CROP_W] > width
        or config[CONF_CROP_Y] + config[CONF_CROP_H] > height
    ):
        raise cv.Invalid(
            f"Crop ({config[CONF_CROP_X]},{config[CONF_CROP_Y]} "
            f"{config[CONF_CROP_W]}x{config[CONF_CROP_H]}) does not fit in the "
            f"decoded {width}x{height} frame",
            path=[CONF_CROP_W],
        )

    config[_KEY_CAMERA_FRAME] = (width, height)
    return config


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    cam = await cg.get_variable(config[CONF_CAMERA_ID])
    decoder = await cg.get_variable(config[CONF_DECODER_ID])
    frame_width, frame_height = config[_KEY_CAMERA_FRAME]
    crop = cg.static_buffer(
        config[CONF_ID], cg.uint8, config[CONF_CROP_W] * config[CONF_CROP_H], psram=True
    )
    var = cg.new_Pvariable(
        config[CONF_ID], cam, decoder, frame_width, frame_height, crop
    )
    await cg.register_component(var, config)
    cg.add_define("USE_SSOCR_READER")

    cg.add(var.set_threshold_level(config[CONF_THRESHOLD_LEVEL]))
    cg.add(
        var.set_crop(
            config[CONF_CROP_X],
            config[CONF_CROP_Y],
            config[CONF_CROP_W],
            config[CONF_CROP_H],
        )
    )
    cg.add(var.set_digit_count(config[CONF_DIGIT_COUNT]))
    cg.add(var.set_debug(config[CONF_DEBUG]))
