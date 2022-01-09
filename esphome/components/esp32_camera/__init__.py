import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.const import (
    CONF_FREQUENCY,
    CONF_ID,
    CONF_PIN,
    CONF_SCL,
    CONF_SDA,
    CONF_DATA_PINS,
    CONF_RESET_PIN,
    CONF_RESOLUTION,
    CONF_BRIGHTNESS,
    CONF_CONTRAST,
)
from esphome.core import CORE
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.cpp_helpers import setup_entity

DEPENDENCIES = ["esp32"]

AUTO_LOAD = ["psram"]

esp32_camera_ns = cg.esphome_ns.namespace("esp32_camera")
ESP32Camera = esp32_camera_ns.class_("ESP32Camera", cg.PollingComponent, cg.EntityBase)
ESP32CameraFrameSize = esp32_camera_ns.enum("ESP32CameraFrameSize")
FRAME_SIZES = {
    "160X120": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_160X120,
    "QQVGA": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_160X120,
    "176X144": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_176X144,
    "QCIF": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_176X144,
    "240X176": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_240X176,
    "HQVGA": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_240X176,
    "320X240": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_320X240,
    "QVGA": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_320X240,
    "400X296": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_400X296,
    "CIF": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_400X296,
    "640X480": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_640X480,
    "VGA": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_640X480,
    "800X600": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_800X600,
    "SVGA": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_800X600,
    "1024X768": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1024X768,
    "XGA": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1024X768,
    "1280X1024": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1280X1024,
    "SXGA": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1280X1024,
    "1600X1200": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1600X1200,
    "UXGA": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1600X1200,
}

CONF_VSYNC_PIN = "vsync_pin"
CONF_HREF_PIN = "href_pin"
CONF_PIXEL_CLOCK_PIN = "pixel_clock_pin"
CONF_EXTERNAL_CLOCK = "external_clock"
CONF_I2C_PINS = "i2c_pins"
CONF_POWER_DOWN_PIN = "power_down_pin"

CONF_MAX_FRAMERATE = "max_framerate"
CONF_IDLE_FRAMERATE = "idle_framerate"
CONF_JPEG_QUALITY = "jpeg_quality"
CONF_VERTICAL_FLIP = "vertical_flip"
CONF_HORIZONTAL_MIRROR = "horizontal_mirror"
CONF_AEC2 = "aec2"
CONF_AE_LEVEL = "ae_level"
CONF_AEC_VALUE = "aec_value"
CONF_SATURATION = "saturation"
CONF_TEST_PATTERN = "test_pattern"

camera_range_param = cv.int_range(min=-2, max=2)

CONFIG_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ESP32Camera),
        cv.Required(CONF_DATA_PINS): cv.All(
            [pins.internal_gpio_input_pin_number], cv.Length(min=8, max=8)
        ),
        cv.Required(CONF_VSYNC_PIN): pins.internal_gpio_input_pin_number,
        cv.Required(CONF_HREF_PIN): pins.internal_gpio_input_pin_number,
        cv.Required(CONF_PIXEL_CLOCK_PIN): pins.internal_gpio_input_pin_number,
        cv.Required(CONF_EXTERNAL_CLOCK): cv.Schema(
            {
                cv.Required(CONF_PIN): pins.internal_gpio_input_pin_number,
                cv.Optional(CONF_FREQUENCY, default="20MHz"): cv.All(
                    cv.frequency, cv.one_of(20e6, 10e6)
                ),
            }
        ),
        cv.Required(CONF_I2C_PINS): cv.Schema(
            {
                cv.Required(CONF_SDA): pins.internal_gpio_output_pin_number,
                cv.Required(CONF_SCL): pins.internal_gpio_output_pin_number,
            }
        ),
        cv.Optional(CONF_RESET_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_POWER_DOWN_PIN): pins.internal_gpio_output_pin_number,
        cv.Optional(CONF_MAX_FRAMERATE, default="10 fps"): cv.All(
            cv.framerate, cv.Range(min=0, min_included=False, max=60)
        ),
        cv.Optional(CONF_IDLE_FRAMERATE, default="0.1 fps"): cv.All(
            cv.framerate, cv.Range(min=0, max=1)
        ),
        cv.Optional(CONF_RESOLUTION, default="640X480"): cv.enum(
            FRAME_SIZES, upper=True
        ),
        cv.Optional(CONF_JPEG_QUALITY, default=10): cv.int_range(min=10, max=63),
        cv.Optional(CONF_CONTRAST, default=0): camera_range_param,
        cv.Optional(CONF_BRIGHTNESS, default=0): camera_range_param,
        cv.Optional(CONF_SATURATION, default=0): camera_range_param,
        cv.Optional(CONF_VERTICAL_FLIP, default=True): cv.boolean,
        cv.Optional(CONF_HORIZONTAL_MIRROR, default=True): cv.boolean,
        cv.Optional(CONF_AEC2, default=False): cv.boolean,
        cv.Optional(CONF_AE_LEVEL, default=0): camera_range_param,
        cv.Optional(CONF_AEC_VALUE, default=300): cv.int_range(min=0, max=1200),
        cv.Optional(CONF_TEST_PATTERN, default=False): cv.boolean,
    }
).extend(cv.COMPONENT_SCHEMA)

SETTERS = {
    CONF_DATA_PINS: "set_data_pins",
    CONF_VSYNC_PIN: "set_vsync_pin",
    CONF_HREF_PIN: "set_href_pin",
    CONF_PIXEL_CLOCK_PIN: "set_pixel_clock_pin",
    CONF_RESET_PIN: "set_reset_pin",
    CONF_POWER_DOWN_PIN: "set_power_down_pin",
    CONF_JPEG_QUALITY: "set_jpeg_quality",
    CONF_VERTICAL_FLIP: "set_vertical_flip",
    CONF_HORIZONTAL_MIRROR: "set_horizontal_mirror",
    CONF_AEC2: "set_aec2",
    CONF_AE_LEVEL: "set_ae_level",
    CONF_AEC_VALUE: "set_aec_value",
    CONF_CONTRAST: "set_contrast",
    CONF_BRIGHTNESS: "set_brightness",
    CONF_SATURATION: "set_saturation",
    CONF_TEST_PATTERN: "set_test_pattern",
}


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await setup_entity(var, config)
    await cg.register_component(var, config)

    for key, setter in SETTERS.items():
        if key in config:
            cg.add(getattr(var, setter)(config[key]))

    extclk = config[CONF_EXTERNAL_CLOCK]
    cg.add(var.set_external_clock(extclk[CONF_PIN], extclk[CONF_FREQUENCY]))
    i2c_pins = config[CONF_I2C_PINS]
    cg.add(var.set_i2c_pins(i2c_pins[CONF_SDA], i2c_pins[CONF_SCL]))
    cg.add(var.set_max_update_interval(1000 / config[CONF_MAX_FRAMERATE]))
    if config[CONF_IDLE_FRAMERATE] == 0:
        cg.add(var.set_idle_update_interval(0))
    else:
        cg.add(var.set_idle_update_interval(1000 / config[CONF_IDLE_FRAMERATE]))
    cg.add(var.set_frame_size(config[CONF_RESOLUTION]))

    cg.add_define("USE_ESP32_CAMERA")

    if CORE.using_esp_idf:
        cg.add_library("espressif/esp32-camera", "1.0.0")
        add_idf_sdkconfig_option("CONFIG_RTCIO_SUPPORT_RTC_GPIO_DESC", True)
