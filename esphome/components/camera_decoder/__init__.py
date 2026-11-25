"""JPEG decoder for camera pipelines."""

import esphome.codegen as cg
from esphome.components import camera, esp32
import esphome.config_validation as cv
from esphome.const import CONF_HEIGHT, CONF_ID, CONF_ROTATION, CONF_WIDTH
from esphome.types import ConfigType

AUTO_LOAD = ["camera"]
CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32"]
DOMAIN = "camera_decoder"

CONF_OUTPUT_TYPE = "output_type"

OUTPUT_TYPES = {
    "GRAYSCALE": camera.PixelFormat.PIXEL_FORMAT_GRAYSCALE,
    "RGB888": camera.PixelFormat.PIXEL_FORMAT_RGB888,
    "RGB565": camera.PixelFormat.PIXEL_FORMAT_RGB565,
    "YUV422": camera.PixelFormat.PIXEL_FORMAT_CBYCRY,
}

# Bytes per pixel of the buffer the library decodes into. GRAYSCALE is decoded as
# RGB888 and compacted afterwards.
BYTES_PER_PIXEL = {
    "GRAYSCALE": 3,
    "RGB888": 3,
    "RGB565": 2,
    "YUV422": 2,
}

# The library decodes whole 16x16 blocks, so the buffer is sized in whole blocks.
JPEG_BLOCK = 16

camera_decoder_ns = cg.esphome_ns.namespace("camera_decoder")
ESP32CameraJPEGDecoder = camera_decoder_ns.class_(
    "ESP32CameraJPEGDecoder", camera.Decoder
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESP32CameraJPEGDecoder),
            cv.Optional(CONF_OUTPUT_TYPE, default="RGB888"): cv.enum(
                OUTPUT_TYPES, upper=True
            ),
            cv.Optional(CONF_ROTATION, default=0): cv.one_of(0, 90, 180, 270),
            # The largest frame to expect. Both set, the output buffer is allocated
            # at boot instead of on the first frame, so no heap is used while running.
            cv.Inclusive(CONF_WIDTH, "frame_size"): cv.int_range(min=1, max=8192),
            cv.Inclusive(CONF_HEIGHT, "frame_size"): cv.int_range(min=1, max=8192),
        }
    ),
    cv.only_on_esp32,
    # esp_new_jpeg 1.0.2 ships no build for these variants, so reject them here
    # instead of failing later in dependency resolution.
    esp32.only_on_variant(
        unsupported=[esp32.VARIANT_ESP32H2, esp32.VARIANT_ESP32H21],
        msg_prefix="The camera decoder",
    ),
)


def _preallocate_size(config: ConfigType) -> int:
    """Bytes to reserve for the largest expected frame, 0 when no size is set."""
    if CONF_WIDTH not in config:
        return 0
    blocks_wide = -(-config[CONF_WIDTH] // JPEG_BLOCK)
    blocks_high = -(-config[CONF_HEIGHT] // JPEG_BLOCK)
    return (
        blocks_wide
        * blocks_high
        * JPEG_BLOCK
        * JPEG_BLOCK
        * BYTES_PER_PIXEL[config[CONF_OUTPUT_TYPE]]
    )


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_CAMERA_DECODER")
    esp32.add_idf_component(name="espressif/esp_new_jpeg", ref="1.0.2")
    preallocate = _preallocate_size(config)
    cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_OUTPUT_TYPE].enum_value,
        config[CONF_ROTATION],
        preallocate,
    )
