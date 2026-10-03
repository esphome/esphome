import esphome.codegen as cg
from esphome.components import camera_video, psram
from esphome.components.const import CONF_BITRATE
from esphome.components.esp32 import (
    add_idf_component,
    add_idf_sdkconfig_option,
    only_on_variant,
)
from esphome.components.esp32.const import VARIANT_ESP32P4
import esphome.config_validation as cv
from esphome.const import CONF_ID
from esphome.types import ConfigType

AUTO_LOAD = ["camera_video"]
DEPENDENCIES = ["esp32", "psram"]
CODEOWNERS = ["@jvgelder"]

camera_h264_ns = cg.esphome_ns.namespace("camera_h264")
CameraH264Encoder = camera_h264_ns.class_(
    "CameraH264Encoder",
    cg.Component,
    camera_video.CameraVideoSourceListener,
    camera_video.H264Stream,
)

CONF_VIDEO_SOURCE_ID = "video_source_id"
CONF_GOP = "gop"
CONF_QP_MIN = "qp_min"
CONF_QP_MAX = "qp_max"
CONF_FPS = "fps"
CONF_ENCODED_FRAME_BUFFERS = "encoded_frame_buffers"
CONF_OUTPUT_BUFFER_SIZE = "output_buffer_size"

MAX_FPS = 60
DEFAULT_BITRATE = 2_000_000
DEFAULT_QP_MIN = 24
DEFAULT_QP_MAX = 40
DEFAULT_ENCODED_FRAME_BUFFERS = 3
MAX_ENCODED_FRAME_BUFFERS = 4
MIN_OUTPUT_BUFFER_SIZE = 64 * 1024
MAX_OUTPUT_BUFFER_SIZE = 4 * 1024 * 1024
ESP_H264_COMPONENT_VERSION = "1.3.8"


def _validate_encoder_config(config: ConfigType) -> ConfigType:
    if config[CONF_QP_MIN] > config[CONF_QP_MAX]:
        raise cv.Invalid("qp_min must be less than or equal to qp_max")
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CameraH264Encoder),
            cv.Required(CONF_VIDEO_SOURCE_ID): cv.use_id(
                camera_video.CameraVideoSource
            ),
            cv.Optional(CONF_FPS): cv.int_range(1, MAX_FPS),
            cv.Optional(CONF_BITRATE, default=DEFAULT_BITRATE): cv.positive_int,
            cv.Optional(CONF_GOP): cv.int_range(1, 255),
            cv.Optional(CONF_QP_MIN, default=DEFAULT_QP_MIN): cv.int_range(0, 51),
            cv.Optional(CONF_QP_MAX, default=DEFAULT_QP_MAX): cv.int_range(0, 51),
            cv.Optional(
                CONF_ENCODED_FRAME_BUFFERS, default=DEFAULT_ENCODED_FRAME_BUFFERS
            ): cv.int_range(1, MAX_ENCODED_FRAME_BUFFERS),
            cv.Optional(CONF_OUTPUT_BUFFER_SIZE): cv.All(
                cv.validate_bytes,
                cv.int_range(MIN_OUTPUT_BUFFER_SIZE, MAX_OUTPUT_BUFFER_SIZE),
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    only_on_variant(supported=[VARIANT_ESP32P4]),
    _validate_encoder_config,
)


def _validate_psram(config: ConfigType) -> ConfigType:
    if not psram.is_guaranteed():
        raise cv.Invalid(
            "camera_h264 requires guaranteed PSRAM; configure psram with "
            "ignore_not_found: false"
        )
    return config


FINAL_VALIDATE_SCHEMA = _validate_psram


async def to_code(config: ConfigType) -> None:
    video_source = await cg.get_variable(config[CONF_VIDEO_SOURCE_ID])
    encoder = cg.new_Pvariable(config[CONF_ID], video_source)
    await cg.register_component(encoder, config)

    if (fps := config.get(CONF_FPS)) is not None:
        cg.add(encoder.set_fps(fps))
    cg.add(encoder.set_bitrate(config[CONF_BITRATE]))
    if (gop := config.get(CONF_GOP)) is not None:
        cg.add(encoder.set_gop(gop))
    cg.add(encoder.set_qp_min(config[CONF_QP_MIN]))
    cg.add(encoder.set_qp_max(config[CONF_QP_MAX]))
    cg.add(encoder.set_encoded_frame_buffers(config[CONF_ENCODED_FRAME_BUFFERS]))
    if (output_buffer_size := config.get(CONF_OUTPUT_BUFFER_SIZE)) is not None:
        cg.add(encoder.set_output_buffer_size(output_buffer_size))

    cg.add_define("USE_CAMERA_H264")
    add_idf_component(name="espressif/esp_h264", ref=ESP_H264_COMPONENT_VERSION)
    add_idf_sdkconfig_option("CONFIG_H264_HW_ENCODER", True)
