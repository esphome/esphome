import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_FORMAT, CONF_HEIGHT, CONF_ID, CONF_TYPE, CONF_WIDTH

CODEOWNERS = ["@DT-art1"]

AUTO_LOAD = ["camera"]

CONF_SENSOR_BUFFER_ID = "sensor_buffer_id"

SOFTWARE_SENSOR = "software"

camera_ns = cg.esphome_ns.namespace("camera")
camera_sensor_ns = cg.esphome_ns.namespace("camera_sensor")
PixelFormat = camera_ns.enum("PixelFormat")

CameraImageSpec = camera_ns.struct("CameraImageSpec")
Sensor = camera_ns.class_("Sensor")
Buffer = camera_ns.class_("Buffer")
BufferImpl = camera_ns.class_("BufferImpl")

SoftwareSensor = camera_sensor_ns.class_("SoftwareSensor", Sensor)

CONF_FORMAT_SELECTS = {
    "GRAYSCALE": PixelFormat.PIXEL_FORMAT_GRAYSCALE,
    "RGB565": PixelFormat.PIXEL_FORMAT_RGB565,
    "BGR888": PixelFormat.PIXEL_FORMAT_BGR888,
}
CONF_IMAGE_SPEC_ID = "image_spec_id"

SOFTWARE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SoftwareSensor),
        cv.Required(CONF_HEIGHT): cv.int_range(0),
        cv.Required(CONF_WIDTH): cv.int_range(0),
        cv.Required(CONF_FORMAT): cv.enum(CONF_FORMAT_SELECTS, upper=True),
        cv.GenerateID(CONF_SENSOR_BUFFER_ID): cv.declare_id(BufferImpl),
        cv.GenerateID(CONF_IMAGE_SPEC_ID): cv.declare_id(CameraImageSpec),
    }
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        SOFTWARE_SENSOR: SOFTWARE_SCHEMA,
    },
    default_type=SOFTWARE_SENSOR,
)


async def to_code(config):
    if config[CONF_TYPE] == SOFTWARE_SENSOR:
        image_spec = cg.new_Pvariable(
            config[CONF_IMAGE_SPEC_ID],
            cg.StructInitializer(
                CameraImageSpec,
                ("width", config[CONF_WIDTH]),
                ("height", config[CONF_HEIGHT]),
                ("format", config[CONF_FORMAT]),
            ),
        )
        buffer = cg.new_Pvariable(
            config[CONF_SENSOR_BUFFER_ID],
            image_spec,
        )
        cg.new_Pvariable(
            config[CONF_ID],
            image_spec,
            buffer,
        )
