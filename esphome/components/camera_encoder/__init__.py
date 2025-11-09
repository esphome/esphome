import esphome.codegen as cg
from esphome.components.esp32 import VARIANT_ESP32P4, add_idf_component, only_on_variant
import esphome.config_validation as cv
from esphome.const import CONF_BUFFER_SIZE, CONF_ID, CONF_TIMEOUT, CONF_TYPE
from esphome.types import ConfigType

CODEOWNERS = ["@DT-art1"]

AUTO_LOAD = ["camera"]

CONF_BUFFER_EXPAND_SIZE = "buffer_expand_size"
CONF_ENCODER_BUFFER_ID = "encoder_buffer_id"
CONF_QUALITY = "quality"
CONF_SUBSAMPLING = "subsampling"

ESP32_CAMERA_ENCODER = "esp32_camera"
ACCELERATED_JPEG = "accelerated_jpeg"

camera_ns = cg.esphome_ns.namespace("camera")
camera_encoder_ns = cg.esphome_ns.namespace("camera_encoder")
Subsampling = camera_ns.enum("Subsampling")

BufferImpl = camera_ns.class_("BufferImpl")
Encoder = camera_ns.class_("Encoder")

ESP32CameraJPEGEncoder = camera_encoder_ns.class_("ESP32CameraJPEGEncoder", Encoder)
AcceleratedJPEGEncoder = camera_encoder_ns.class_("AcceleratedJPEGEncoder", Encoder)

BUFFER_SIZE_1MB = 1024 * 1024
MAX_JPEG_BUFFER_SIZE_2MB = 2 * BUFFER_SIZE_1MB

ESP32_CAMERA_ENCODER_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ESP32CameraJPEGEncoder),
        cv.Optional(CONF_QUALITY, default=40): cv.int_range(1, 100),
        cv.Optional(CONF_BUFFER_SIZE, default=4096): cv.Any(
            0, cv.int_range(1024, MAX_JPEG_BUFFER_SIZE_2MB)
        ),
        cv.Optional(CONF_BUFFER_EXPAND_SIZE, default=1024): cv.int_range(
            0, MAX_JPEG_BUFFER_SIZE_2MB
        ),
        cv.GenerateID(CONF_ENCODER_BUFFER_ID): cv.declare_id(BufferImpl),
    }
)

CONF_ESP32P4_SUBSAMPLING_SELECTS = {
    "444": Subsampling.SUBSAMPLING_444,
    "422": Subsampling.SUBSAMPLING_422,
    "420": Subsampling.SUBSAMPLING_420,
}
ACCELERATED_JPEG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(AcceleratedJPEGEncoder),
            cv.Optional(CONF_QUALITY, default=40): cv.int_range(1, 100),
            cv.Optional(CONF_SUBSAMPLING, default="444"): cv.enum(
                CONF_ESP32P4_SUBSAMPLING_SELECTS, upper=True
            ),
            cv.Optional(CONF_BUFFER_SIZE, default=BUFFER_SIZE_1MB): cv.Any(
                0, cv.int_range(1024, MAX_JPEG_BUFFER_SIZE_2MB)
            ),
            cv.Optional(CONF_TIMEOUT, default=100): cv.int_range(10, 4000),
            cv.GenerateID(CONF_ENCODER_BUFFER_ID): cv.declare_id(BufferImpl),
        },
    ),
    only_on_variant(supported=[VARIANT_ESP32P4]),
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        ESP32_CAMERA_ENCODER: ESP32_CAMERA_ENCODER_SCHEMA,
        ACCELERATED_JPEG: ACCELERATED_JPEG_SCHEMA,
    },
    default_type=ESP32_CAMERA_ENCODER,
)


async def to_code(config: ConfigType) -> None:
    if config[CONF_TYPE] == ESP32_CAMERA_ENCODER:
        add_idf_component(name="espressif/esp32-camera", ref="2.1.1")
        cg.add_define("USE_ESP32_CAMERA_JPEG_ENCODER")
        var = cg.new_Pvariable(
            config[CONF_ID],
            config[CONF_QUALITY],
        )
        cg.add(var.set_buffer_expand_size(config[CONF_BUFFER_EXPAND_SIZE]))
    if config[CONF_TYPE] == ACCELERATED_JPEG:
        cg.add_build_flag("-DUSE_P4_CAMERA_JPEG_ENCODER")
        var = cg.new_Pvariable(
            config[CONF_ID],
            config[CONF_QUALITY],
            config[CONF_SUBSAMPLING],
            config[CONF_TIMEOUT],
        )

    buffer = cg.new_Pvariable(config[CONF_ENCODER_BUFFER_ID])
    cg.add(buffer.set_buffer_size(config[CONF_BUFFER_SIZE]))
    cg.add(var.set_output_buffer(buffer))
