"""JPEG decoder for camera pipelines."""

import esphome.codegen as cg
from esphome.components import camera, esp32
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_ROTATION
from esphome.types import ConfigType

AUTO_LOAD = ["camera"]
CODEOWNERS = ["@nliaudat"]
DEPENDENCIES = ["esp32"]
DOMAIN = "camera_decoder"

CONF_OUTPUT_TYPE = "output_type"

OUTPUT_TYPES = {
    "RGB888": camera.PixelFormat.PIXEL_FORMAT_RGB888,
    "RGB565": camera.PixelFormat.PIXEL_FORMAT_RGB565,
    "YUV422": camera.PixelFormat.PIXEL_FORMAT_CBYCRY,
}

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


async def to_code(config: ConfigType) -> None:
    cg.add_define("USE_CAMERA_DECODER")
    esp32.add_idf_component(name="espressif/esp_new_jpeg", ref="1.0.2")
    cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_OUTPUT_TYPE].enum_value,
        config[CONF_ROTATION],
    )
