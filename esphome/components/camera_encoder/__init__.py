import esphome.codegen as cg
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    add_idf_component,
    get_esp32_variant,
)
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID, CONF_TYPE
from esphome.core import CORE
from esphome.types import ConfigType

CODEOWNERS = ["@DT-art1"]

AUTO_LOAD = ["camera"]

CONF_BUFFER_EXPAND_SIZE = "buffer_expand_size"
CONF_ENCODER_BUFFER_ID = "encoder_buffer_id"
CONF_QUALITY = "quality"
CONF_MCU_COUNT = "mcu_count"
CONF_SUBSAMPLING = "subsampling"

ESP32_CAMERA_ENCODER = "esp32_camera"
P4_ENCODER = "p4"
BITBANK2_ENCODER = "bitbank2"

camera_ns = cg.esphome_ns.namespace("camera")
camera_encoder_ns = cg.esphome_ns.namespace("camera_encoder")
Subsampling = camera_ns.enum("Subsampling")

Encoder = camera_ns.class_("Encoder")
EncoderBufferImpl = camera_encoder_ns.class_("EncoderBufferImpl")

ESP32CameraJPEGEncoder = camera_encoder_ns.class_("ESP32CameraJPEGEncoder", Encoder)

ESP32P4JPEGEncoder = camera_encoder_ns.class_("ESP32P4JPEGEncoder", Encoder)
ESP32P4EncoderBuffer = camera_encoder_ns.class_(
    "ESP32P4EncoderBuffer", EncoderBufferImpl
)

Bitbank2Encoder = camera_encoder_ns.class_("Bitbank2JPEGEncoder", Encoder)
Bitbank2Quality = camera_encoder_ns.enum("Bitbank2Quality")
MAX_JPEG_BUFFER_SIZE_2MB = 2 * 1024 * 1024

ESP32_CAMERA_ENCODER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32CameraJPEGEncoder),
        cv.Optional(CONF_QUALITY, default=80): cv.int_range(1, 100),
        cv.Optional(CONF_BUFFER_SIZE, default=4096): cv.int_range(
            1024, MAX_JPEG_BUFFER_SIZE_2MB
        ),
        cv.Optional(CONF_BUFFER_EXPAND_SIZE, default=1024): cv.int_range(
            0, MAX_JPEG_BUFFER_SIZE_2MB
        ),
        cv.GenerateID(CONF_ENCODER_BUFFER_ID): cv.declare_id(EncoderBufferImpl),
    }
)

CONF_ESP32P4_SUBSAMPLING_SELECTS = {
    "444": Subsampling.SUBSAMPLING_444,
    "422": Subsampling.SUBSAMPLING_422,
    "420": Subsampling.SUBSAMPLING_420,
}
P4_ENCODER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32P4JPEGEncoder),
        cv.Optional(CONF_QUALITY, default=80): cv.int_range(1, 100),
        cv.Optional(CONF_SUBSAMPLING, default="444"): cv.enum(
            CONF_ESP32P4_SUBSAMPLING_SELECTS, upper=True
        ),
        cv.Optional(CONF_BUFFER_SIZE, default=10240): cv.int_range(
            1024, MAX_JPEG_BUFFER_SIZE_2MB
        ),
        cv.GenerateID(CONF_ENCODER_BUFFER_ID): cv.declare_id(ESP32P4EncoderBuffer),
    },
)

CONF_BITBANK2_SUBSAMPLING_SELECTS = {
    "444": Subsampling.SUBSAMPLING_444,
    "420": Subsampling.SUBSAMPLING_420,
}
CONF_BITBANK2_QUALITY_SELECTS = {
    "BEST": Bitbank2Quality.QUALITY_BEST,
    "HIGH": Bitbank2Quality.QUALITY_HIGH,
    "MED": Bitbank2Quality.QUALITY_MED,
    "LOW": Bitbank2Quality.QUALITY_LOW,
}
BITBANK2_ENCODER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(Bitbank2Encoder),
        cv.Optional(CONF_QUALITY, default="HIGH"): cv.enum(
            CONF_BITBANK2_QUALITY_SELECTS, upper=True
        ),
        cv.Optional(CONF_SUBSAMPLING, default="444"): cv.enum(
            CONF_BITBANK2_SUBSAMPLING_SELECTS, upper=True
        ),
        cv.Optional(CONF_MCU_COUNT, default=0): cv.int_range(0),
        cv.Optional(CONF_BUFFER_SIZE, default=4096): cv.int_range(
            1024, MAX_JPEG_BUFFER_SIZE_2MB
        ),
        cv.Optional(CONF_BUFFER_EXPAND_SIZE, default=1024): cv.int_range(
            0, MAX_JPEG_BUFFER_SIZE_2MB
        ),
        cv.GenerateID(CONF_ENCODER_BUFFER_ID): cv.declare_id(EncoderBufferImpl),
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        ESP32_CAMERA_ENCODER: ESP32_CAMERA_ENCODER_SCHEMA,
        P4_ENCODER: P4_ENCODER_SCHEMA,
        BITBANK2_ENCODER: BITBANK2_ENCODER_SCHEMA,
    },
    default_type=ESP32_CAMERA_ENCODER,
)


def _final_validate(config):
    if config[CONF_TYPE] == P4_ENCODER and get_esp32_variant() != VARIANT_ESP32P4:
        raise cv.Invalid(
            f"{config[CONF_TYPE]} requires ESP32P4.",
        )


FINAL_VALIDATE_SCHEMA = _final_validate


async def to_code(config: ConfigType) -> None:
    buffer = cg.new_Pvariable(config[CONF_ENCODER_BUFFER_ID])
    cg.add(buffer.set_buffer_size(config[CONF_BUFFER_SIZE]))
    if config[CONF_TYPE] == ESP32_CAMERA_ENCODER:
        if CORE.using_esp_idf:
            add_idf_component(name="espressif/esp32-camera", ref="2.1.2")
        cg.add_build_flag("-DUSE_ESP32_CAMERA_JPEG_ENCODER")
        var = cg.new_Pvariable(
            config[CONF_ID],
            config[CONF_QUALITY],
            buffer,
        )
        cg.add(var.set_buffer_expand_size(config[CONF_BUFFER_EXPAND_SIZE]))
    if config[CONF_TYPE] == P4_ENCODER:
        cg.add_build_flag("-DUSE_P4_CAMERA_JPEG_ENCODER")
        var = cg.new_Pvariable(
            config[CONF_ID],
            config[CONF_QUALITY],
            config[CONF_SUBSAMPLING],
            buffer,
        )
    if config[CONF_TYPE] == BITBANK2_ENCODER:
        cg.add_build_flag("-DUSE_BITBANK2_JPEG_ENCODER")
        cg.add_library("dt-art1/jpegenc-pio", "1.0.0")
        var = cg.new_Pvariable(
            config[CONF_ID],
            config[CONF_QUALITY],
            config[CONF_SUBSAMPLING],
            config[CONF_MCU_COUNT],
            buffer,
        )
        cg.add(var.set_buffer_expand_size(config[CONF_BUFFER_EXPAND_SIZE]))
