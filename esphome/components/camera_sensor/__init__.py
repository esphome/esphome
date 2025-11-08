from esphome import pins
import esphome.codegen as cg
from esphome.components import i2c
from esphome.components.esp32 import (
    VARIANT_ESP32P4,
    add_idf_component,
    add_idf_sdkconfig_option,
    only_on_variant,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_BLUE,
    CONF_BRIGHTNESS,
    CONF_CLEAR,
    CONF_CONTRAST,
    CONF_FILTER,
    CONF_FLIP_X,
    CONF_FLIP_Y,
    CONF_FORMAT,
    CONF_FREQUENCY,
    CONF_GAIN,
    CONF_GREEN,
    CONF_HEIGHT,
    CONF_I2C_ID,
    CONF_MODE,
    CONF_PINS,
    CONF_RED,
    CONF_RESET,
    CONF_TYPE,
    CONF_WIDTH,
)

CODEOWNERS = ["@DT-art1"]

AUTO_LOAD = ["camera", "i2c"]

SOFTWARE_SENSOR = "software"
ESP32_CAMERA_SENSOR = "esp32_camera"
MIPI_CSI = "mipi_csi"

CONF_CAMERA_SENSOR_ID = "camera_sensor_id"
CONF_ISP_ID = "isp_id"

camera_ns = cg.esphome_ns.namespace("camera")
camera_sensor_ns = cg.esphome_ns.namespace("camera_sensor")
PixelFormat = camera_ns.enum("PixelFormat")
BayerPattern = camera_sensor_ns.enum("BayerPattern")

Sensor = camera_ns.class_("Sensor")

SoftwareSensor = camera_sensor_ns.class_("SoftwareSensor", Sensor)
ESP32CameraSensor = camera_sensor_ns.class_("ESP32CameraSensor", Sensor)
CSICameraSensor = camera_sensor_ns.class_("CSICameraSensor", Sensor)
ISP = camera_sensor_ns.class_("ISP")

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

CONF_JPEG_QUALITY = "jpeg_quality"

CONF_BAYER_PATTERN = "bayer_pattern"
CONF_BUFFERS = "buffers"
CONF_BYTE_SWAP = "byte_swap"
CONF_EXPOSURE = "exposure"
CONF_FACTORY_FLIP_X = "factory_flip_x"
CONF_HUE = "hue"
CONF_SATURATION = "saturation"
CONF_TEST_PATTERN = "test_pattern"

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

CONF_BAYER_PATTERN_SELECTS = {
    "AUTO": BayerPattern.AUTO,
    "RGGB": BayerPattern.RGGB,
    "GRBG": BayerPattern.GRBG,
    "GBRG": BayerPattern.GBRG,
    "BGGR": BayerPattern.BGGR,
}

SOFTWARE_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CAMERA_SENSOR_ID): cv.declare_id(SoftwareSensor),
        cv.Required(CONF_HEIGHT): cv.int_range(0),
        cv.Required(CONF_WIDTH): cv.int_range(0),
        cv.Required(CONF_FORMAT): cv.enum(CONF_FORMAT_SELECTS, upper=True),
        cv.Optional(CONF_BUFFERS, default=1): cv.int_range(1, 2),
        cv.Optional(CONF_CLEAR, default=True): cv.boolean,
    }
)

ESP32_CAMERA_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_CAMERA_SENSOR_ID): cv.declare_id(ESP32CameraSensor),
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
            cv.Optional(CONF_FORMAT): cv.enum(CONF_FORMAT_SELECTS, upper=True),
            cv.Optional(CONF_BUFFERS, default=2): cv.int_range(1, 2),
            cv.Optional(CONF_JPEG_QUALITY): cv.int_range(0, 100),
            cv.Optional(CONF_FLIP_X, default=False): cv.boolean,
            cv.Optional(CONF_FLIP_Y, default=False): cv.boolean,
        }
    ),
    cv.has_exactly_one_key(CONF_FORMAT, CONF_JPEG_QUALITY),
    validate_resolution,
)

CONF_RESTRICTED_PIXEL_FORMAT_SELECTS = {
    "RGB565": PixelFormat.PIXEL_FORMAT_RGB565,
    "BGR888": PixelFormat.PIXEL_FORMAT_BGR888,
}

MIPI_CSI_SCHEMA = cv.Schema(
    {
        cv.GenerateID(CONF_CAMERA_SENSOR_ID): cv.declare_id(CSICameraSensor),
        cv.Required(CONF_MODE): cv.string,
        cv.Optional(CONF_FORMAT, default="RGB565"): cv.enum(
            CONF_RESTRICTED_PIXEL_FORMAT_SELECTS, upper=True
        ),
        cv.Optional(CONF_BAYER_PATTERN, default="AUTO"): cv.enum(
            CONF_BAYER_PATTERN_SELECTS, upper=True
        ),
        cv.Optional(CONF_PWDN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_RESET): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_XCLK): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_FREQUENCY, default="24MHz"): cv.All(
            cv.frequency, cv.Range(min=6e6, max=48e6)
        ),
        cv.Optional(CONF_BUFFERS, default=2): cv.int_range(1, 10),
        cv.Optional(CONF_BYTE_SWAP, default=False): cv.boolean,
        cv.Optional(CONF_FACTORY_FLIP_X, default=True): cv.boolean,
        cv.Optional(CONF_FLIP_X): cv.boolean,
        cv.Optional(CONF_FLIP_Y): cv.boolean,
        cv.Optional(CONF_BRIGHTNESS, default=0): cv.int_range(-127, 128),
        cv.Optional(CONF_CONTRAST, default=128): cv.int_range(0, 128),
        cv.Optional(CONF_FILTER, default=10): cv.int_range(2, 20),
        cv.Optional(CONF_HUE, default=0): cv.int_range(0, 359),
        cv.Optional(CONF_SATURATION, default=128): cv.int_range(0, 128),
        cv.Optional(CONF_RED, default=1.0): cv.float_range(-4.0, 4.0),
        cv.Optional(CONF_GREEN, default=1.0): cv.float_range(-4.0, 4.0),
        cv.Optional(CONF_BLUE, default=1.0): cv.float_range(-4.0, 4.0),
        cv.Optional(CONF_GAIN): cv.int_range(0),
        cv.Optional(CONF_EXPOSURE): cv.int_range(0),
        cv.Optional(CONF_TEST_PATTERN): cv.boolean,
        cv.GenerateID(CONF_ISP_ID): cv.declare_id(ISP),
    }
)


CONFIG_SCHEMA = cv.typed_schema(
    {
        SOFTWARE_SENSOR: SOFTWARE_SCHEMA,
        ESP32_CAMERA_SENSOR: ESP32_CAMERA_SCHEMA,
        MIPI_CSI: cv.All(
            MIPI_CSI_SCHEMA.extend(i2c.i2c_device_schema(0x00)),
            only_on_variant(supported=[VARIANT_ESP32P4]),
        ),
    },
    default_type=SOFTWARE_SENSOR,
)


async def to_code(config):
    if config[CONF_TYPE] == SOFTWARE_SENSOR:
        var = cg.new_Pvariable(
            config[CONF_CAMERA_SENSOR_ID],
            config[CONF_WIDTH],
            config[CONF_HEIGHT],
            config[CONF_FORMAT],
        )
        cg.add(var.set_buffers(config[CONF_BUFFERS]))
        cg.add(var.set_clear(config[CONF_CLEAR]))
    if config[CONF_TYPE] == ESP32_CAMERA_SENSOR:
        add_idf_component(name="espressif/esp32-camera", ref="2.1.2")
        cg.add_build_flag("-DUSE_ESP32_CAMERA_SENSOR")
        cg.add_build_flag("-DCONFIG_CAMERA_PSRAM_DMA=1")
        var = cg.new_Pvariable(
            config[CONF_CAMERA_SENSOR_ID],
            config[CONF_WIDTH],
            config[CONF_HEIGHT],
        )
        reset_pin = config.get(CONF_RESET, -1)
        pwdn_pin = config.get(CONF_PWDN, -1)
        cg.add(
            var.set_pins(
                config[CONF_PINS][CONF_D0],
                config[CONF_PINS][CONF_D1],
                config[CONF_PINS][CONF_D2],
                config[CONF_PINS][CONF_D3],
                config[CONF_PINS][CONF_D4],
                config[CONF_PINS][CONF_D5],
                config[CONF_PINS][CONF_D6],
                config[CONF_PINS][CONF_D7],
                config[CONF_PINS][CONF_XCLK],
                config[CONF_PINS][CONF_VSYNC],
                config[CONF_PINS][CONF_HREF],
                config[CONF_PINS][CONF_PCLK],
                pwdn_pin,
                reset_pin,
            )
        )
        i2c_bus = await cg.get_variable(config.get(CONF_I2C_ID))
        cg.add(var.set_i2c_bus(i2c_bus))
        cg.add(var.set_frequency(config[CONF_FREQUENCY]))
        resolution = (config[CONF_WIDTH], config[CONF_HEIGHT])
        cg.add(var.set_framesize(FRAME_SIZES[resolution]))
        cg.add(var.set_buffers(config[CONF_BUFFERS]))
        cg.add(var.set_flip_x(config[CONF_FLIP_X]))
        cg.add(var.set_flip_y(config[CONF_FLIP_Y]))
        if CONF_FORMAT in config:
            cg.add(var.set_pixel_format(config[CONF_FORMAT]))
        if CONF_JPEG_QUALITY in config:
            cg.add(var.set_jpeg_quality(config[CONF_JPEG_QUALITY]))

    if config[CONF_TYPE] == MIPI_CSI:
        add_idf_component(name="espressif/esp_cam_sensor", ref="1.5.1")
        cg.add_build_flag("-DUSE_CSI_CAMERA_SENSOR")
        # Experimental features required for camera sensors that output RAW10/RAW12
        # and for the ISP to output RGB888
        add_idf_sdkconfig_option("CONFIG_IDF_EXPERIMENTAL_FEATURES", True)
        isp = cg.new_Pvariable(config[CONF_ISP_ID])
        cg.add(isp.set_brightness(config[CONF_BRIGHTNESS]))
        cg.add(isp.set_contrast(config[CONF_CONTRAST]))
        cg.add(isp.set_filter(config[CONF_FILTER]))
        cg.add(isp.set_hue(config[CONF_HUE]))
        cg.add(isp.set_saturation(config[CONF_SATURATION]))
        cg.add(isp.set_bayer_pattern(config[CONF_BAYER_PATTERN]))
        cg.add(isp.set_red(config[CONF_RED]))
        cg.add(isp.set_green(config[CONF_GREEN]))
        cg.add(isp.set_blue(config[CONF_BLUE]))
        var = cg.new_Pvariable(
            config[CONF_CAMERA_SENSOR_ID],
            config[CONF_MODE],
            config[CONF_FORMAT],
        )
        xclk_pin = config.get(CONF_XCLK, -1)
        pwdn_pin = config.get(CONF_PWDN, -1)
        reset_pin = config.get(CONF_RESET, -1)
        cg.add(var.set_pins(xclk_pin, pwdn_pin, reset_pin))
        cg.add(var.set_buffers(config[CONF_BUFFERS]))
        cg.add(var.set_byte_swap(config[CONF_BYTE_SWAP]))
        cg.add(var.set_frequency(config[CONF_FREQUENCY]))
        cg.add(var.set_factory_flip_x(config[CONF_FACTORY_FLIP_X]))
        cg.add(var.set_isp(isp))
        if CONF_TEST_PATTERN in config:
            cg.add(var.set_test_pattern(config[CONF_TEST_PATTERN]))
        if CONF_FLIP_X in config:
            cg.add(var.set_flip_x(config[CONF_FLIP_X]))
        if CONF_FLIP_Y in config:
            cg.add(var.set_flip_y(config[CONF_FLIP_Y]))
        if CONF_GAIN in config:
            cg.add(var.set_gain(config[CONF_GAIN]))
        if CONF_EXPOSURE in config:
            cg.add(var.set_exposure(config[CONF_EXPOSURE]))
        await i2c.register_i2c_device(var, config)
