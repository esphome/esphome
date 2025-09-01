from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.esp32 import add_idf_component
import esphome.config_validation as cv
from esphome.const import (
    CONF_FORMAT,
    CONF_FREQUENCY,
    CONF_HEIGHT,
    CONF_I2C_ID,
    CONF_ID,
    CONF_PINS,
    CONF_RESET,
    CONF_TYPE,
    CONF_WIDTH,
)
from esphome.core import CORE

CODEOWNERS = ["@DT-art1"]

AUTO_LOAD = ["camera"]

CONF_SENSOR_BUFFER_ID = "sensor_buffer_id"

SOFTWARE_SENSOR = "software"
ESP32_CAMERA_SENSOR = "esp32_camera"

camera_ns = cg.esphome_ns.namespace("camera")
camera_sensor_ns = cg.esphome_ns.namespace("camera_sensor")
PixelFormat = camera_ns.enum("PixelFormat")

CameraImageSpec = camera_ns.struct("CameraImageSpec")
Sensor = camera_ns.class_("Sensor")
Buffer = camera_ns.class_("Buffer")
BufferImpl = camera_ns.class_("BufferImpl")

SoftwareSensor = camera_sensor_ns.class_("SoftwareSensor", Sensor)
ESP32CameraSensor = camera_sensor_ns.class_("ESP32CameraSensor", Sensor)

CONF_D0 = "d0"
CONF_D1 = "d1"
CONF_D2 = "d2"
CONF_D3 = "d3"
CONF_D4 = "d4"
CONF_D5 = "d5"
CONF_D6 = "d6"
CONF_D7 = "d7"
CONF_HREF = "href"
CONF_PCLK = "pclk"
CONF_PWDN = "pwdn"
CONF_VSYNC = "vsync"
CONF_XCLK = "xclk"

PINS_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_PWDN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_RESET): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_XCLK): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_VSYNC): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_HREF): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_PCLK): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D7): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D6): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D5): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D4): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D3): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D2): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D1): pins.internal_gpio_output_pin_number,
        cv.Required(CONF_D0): pins.internal_gpio_output_pin_number,
    }
)

FRAME_SIZES = {
    (96, 96): cg.RawExpression("FRAMESIZE_96X96"),
    (160, 120): cg.RawExpression("FRAMESIZE_QQVGA"),
    (128, 128): cg.RawExpression("FRAMESIZE_128X128"),
    (176, 144): cg.RawExpression("FRAMESIZE_QCIF"),
    (240, 176): cg.RawExpression("FRAMESIZE_HQVGA"),
    (240, 240): cg.RawExpression("FRAMESIZE_240X240"),
    (320, 240): cg.RawExpression("FRAMESIZE_QVGA"),
    (320, 320): cg.RawExpression("FRAMESIZE_320X320"),
    (400, 296): cg.RawExpression("FRAMESIZE_CIF"),
    (480, 320): cg.RawExpression("FRAMESIZE_HVGA"),
    (640, 480): cg.RawExpression("FRAMESIZE_VGA"),
    (800, 600): cg.RawExpression("FRAMESIZE_SVGA"),
    (1024, 768): cg.RawExpression("FRAMESIZE_XGA"),
    (1280, 720): cg.RawExpression("FRAMESIZE_HD"),
    (1280, 1024): cg.RawExpression("FRAMESIZE_SXGA"),
    (1600, 1200): cg.RawExpression("FRAMESIZE_UXGA"),
    (1920, 1080): cg.RawExpression("FRAMESIZE_FHD"),
    (720, 1280): cg.RawExpression("FRAMESIZE_P_HD"),
    (864, 1536): cg.RawExpression("FRAMESIZE_P_3MP"),
    (2048, 1536): cg.RawExpression("FRAMESIZE_QXGA"),
    (2560, 1440): cg.RawExpression("FRAMESIZE_QHD"),
    (2560, 1600): cg.RawExpression("FRAMESIZE_WQXGA"),
    (1080, 1920): cg.RawExpression("FRAMESIZE_P_FHD"),
    (2560, 1920): cg.RawExpression("FRAMESIZE_QSXGA"),
    (2592, 1944): cg.RawExpression("FRAMESIZE_5MP"),
}


def validate_resolution(config):
    resolution = (config[CONF_WIDTH], config[CONF_HEIGHT])
    if resolution not in FRAME_SIZES:
        raise cv.Invalid(
            f"Invalid resolution {resolution}. Allowed resolutions: {list(FRAME_SIZES.keys())}"
        )
    return config


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

ESP32_CAMERA_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ESP32CameraSensor),
            cv.Required(CONF_PINS): PINS_SCHEMA,
            cv.Required(CONF_I2C_ID): cv.Any(
                cv.use_id(i2c.InternalI2CBus),
                msg="I2C bus must be an internal ESP32 I2C bus",
            ),
            cv.Optional(CONF_FREQUENCY, default="16MHz"): cv.All(
                cv.frequency, cv.Range(min=8e6, max=20e6)
            ),
            cv.Required(CONF_HEIGHT): cv.int_range(0),
            cv.Required(CONF_WIDTH): cv.int_range(0),
            cv.Required(CONF_FORMAT): cv.enum(CONF_FORMAT_SELECTS, upper=True),
            cv.GenerateID(CONF_IMAGE_SPEC_ID): cv.declare_id(CameraImageSpec),
        }
    ),
    validate_resolution,
)

CONFIG_SCHEMA = cv.typed_schema(
    {
        SOFTWARE_SENSOR: SOFTWARE_SCHEMA,
        ESP32_CAMERA_SENSOR: ESP32_CAMERA_SCHEMA,
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
    if config[CONF_TYPE] == ESP32_CAMERA_SENSOR:
        if CORE.using_esp_idf:
            add_idf_component(name="espressif/esp32-camera", ref="2.1.2")
        image_spec = cg.new_Pvariable(
            config[CONF_IMAGE_SPEC_ID],
            cg.StructInitializer(
                CameraImageSpec,
                ("width", config[CONF_WIDTH]),
                ("height", config[CONF_HEIGHT]),
                ("format", config[CONF_FORMAT]),
            ),
        )
        var = cg.new_Pvariable(
            config[CONF_ID],
            image_spec,
        )
        pins = config[CONF_PINS]
        reset_pin = config.get(CONF_RESET, -1)
        pwdn_pin = config.get(CONF_PWDN, -1)
        cg.add(
            var.set_pins(
                pins[CONF_D0],
                pins[CONF_D1],
                pins[CONF_D2],
                pins[CONF_D3],
                pins[CONF_D4],
                pins[CONF_D5],
                pins[CONF_D6],
                pins[CONF_D7],
                pins[CONF_XCLK],
                pins[CONF_VSYNC],
                pins[CONF_HREF],
                pins[CONF_PCLK],
                pwdn_pin,
                reset_pin,
            )
        )
        i2c_bus = await cg.get_variable(config.get(CONF_I2C_ID))
        cg.add(var.set_i2c_bus(i2c_bus))
        cg.add(var.set_frequency(config[CONF_FREQUENCY]))
        resolution = (config[CONF_WIDTH], config[CONF_HEIGHT])
        cg.add(var.set_framesize(FRAME_SIZES[resolution]))
