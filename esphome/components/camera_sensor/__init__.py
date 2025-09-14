import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_FORMAT, CONF_HEIGHT, CONF_ID, CONF_TYPE, CONF_WIDTH

CODEOWNERS = ["@DT-art1"]

AUTO_LOAD = ["camera"]

SOFTWARE_SENSOR = "software"

camera_ns = cg.esphome_ns.namespace("camera")
camera_sensor_ns = cg.esphome_ns.namespace("camera_sensor")
PixelFormat = camera_ns.enum("PixelFormat")

Sensor = camera_ns.class_("Sensor")

SoftwareSensor = camera_sensor_ns.class_("SoftwareSensor", Sensor)

CONF_BUFFERS = "buffers"
CONF_CLEAR = "clear"

CONF_FORMAT_SELECTS = {
    "GRAYSCALE": PixelFormat.PIXEL_FORMAT_GRAYSCALE,
    "RGB565": PixelFormat.PIXEL_FORMAT_RGB565,
    "BGR888": PixelFormat.PIXEL_FORMAT_BGR888,
}

SOFTWARE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(SoftwareSensor),
        cv.Required(CONF_HEIGHT): cv.int_range(0),
        cv.Required(CONF_WIDTH): cv.int_range(0),
        cv.Required(CONF_FORMAT): cv.enum(CONF_FORMAT_SELECTS, upper=True),
        cv.Optional(CONF_BUFFERS, default=1): cv.int_range(1, 2),
        cv.Optional(CONF_CLEAR, default=True): cv.boolean,
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
        var = cg.new_Pvariable(
            config[CONF_ID],
            config[CONF_WIDTH],
            config[CONF_HEIGHT],
            config[CONF_FORMAT],
        )
        cg.add(var.set_buffers(config[CONF_BUFFERS]))
        cg.add(var.set_clear(config[CONF_CLEAR]))
