import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import automation
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
    CONF_TRIGGER_ID,
    CONF_VSYNC_PIN,
)
from esphome.core import CORE
from esphome.components.esp32 import add_idf_sdkconfig_option
from esphome.cpp_helpers import setup_entity

DEPENDENCIES = ["esp32"]

AUTO_LOAD = ["psram"]

esp32_camera_ns = cg.esphome_ns.namespace("esp32_camera")
ESP32Camera = esp32_camera_ns.class_("ESP32Camera", cg.PollingComponent, cg.EntityBase)
ESP32CameraImageData = esp32_camera_ns.struct("CameraImageData")
# Triggers
ESP32CameraImageTrigger = esp32_camera_ns.class_(
    "ESP32CameraImageTrigger", automation.Trigger.template()
)
ESP32CameraStreamStartTrigger = esp32_camera_ns.class_(
    "ESP32CameraStreamStartTrigger",
    automation.Trigger.template(),
)
ESP32CameraStreamStopTrigger = esp32_camera_ns.class_(
    "ESP32CameraStreamStopTrigger",
    automation.Trigger.template(),
)
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
    "1920X1080": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1920X1080,
    "FHD": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1920X1080,
    "720X1280": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_720X1280,
    "PHD": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_720X1280,
    "864X1536": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_864X1536,
    "P3MP": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_864X1536,
    "2048X1536": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_2048X1536,
    "QXGA": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_2048X1536,
    "2560X1440": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_2560X1440,
    "QHD": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_2560X1440,
    "2560X1600": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_2560X1600,
    "WQXGA": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_2560X1600,
    "1080X1920": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1080X1920,
    "PFHD": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1080X1920,
    "2560X1920": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_2560X1920,
    "QSXGA": ESP32CameraFrameSize.ESP32_CAMERA_SIZE_2560X1920,
}
ESP32GainControlMode = esp32_camera_ns.enum("ESP32GainControlMode")
ENUM_GAIN_CONTROL_MODE = {
    "MANUAL": ESP32GainControlMode.ESP32_GC_MODE_MANU,
    "AUTO": ESP32GainControlMode.ESP32_GC_MODE_AUTO,
}
ESP32AgcGainCeiling = esp32_camera_ns.enum("ESP32AgcGainCeiling")
ENUM_GAIN_CEILING = {
    "2X": ESP32AgcGainCeiling.ESP32_GAINCEILING_2X,
    "4X": ESP32AgcGainCeiling.ESP32_GAINCEILING_4X,
    "8X": ESP32AgcGainCeiling.ESP32_GAINCEILING_8X,
    "16X": ESP32AgcGainCeiling.ESP32_GAINCEILING_16X,
    "32X": ESP32AgcGainCeiling.ESP32_GAINCEILING_32X,
    "64X": ESP32AgcGainCeiling.ESP32_GAINCEILING_64X,
    "128X": ESP32AgcGainCeiling.ESP32_GAINCEILING_128X,
}
ESP32WhiteBalanceMode = esp32_camera_ns.enum("ESP32WhiteBalanceMode")
ENUM_WB_MODE = {
    "AUTO": ESP32WhiteBalanceMode.ESP32_WB_MODE_AUTO,
    "SUNNY": ESP32WhiteBalanceMode.ESP32_WB_MODE_SUNNY,
    "CLOUDY": ESP32WhiteBalanceMode.ESP32_WB_MODE_CLOUDY,
    "OFFICE": ESP32WhiteBalanceMode.ESP32_WB_MODE_OFFICE,
    "HOME": ESP32WhiteBalanceMode.ESP32_WB_MODE_HOME,
}
ESP32SpecialEffect = esp32_camera_ns.enum("ESP32SpecialEffect")
ENUM_SPECIAL_EFFECT = {
    "NONE": ESP32SpecialEffect.ESP32_SPECIAL_EFFECT_NONE,
    "NEGATIVE": ESP32SpecialEffect.ESP32_SPECIAL_EFFECT_NEGATIVE,
    "GRAYSCALE": ESP32SpecialEffect.ESP32_SPECIAL_EFFECT_GRAYSCALE,
    "RED_TINT": ESP32SpecialEffect.ESP32_SPECIAL_EFFECT_RED_TINT,
    "GREEN_TINT": ESP32SpecialEffect.ESP32_SPECIAL_EFFECT_GREEN_TINT,
    "BLUE_TINT": ESP32SpecialEffect.ESP32_SPECIAL_EFFECT_BLUE_TINT,
    "SEPIA": ESP32SpecialEffect.ESP32_SPECIAL_EFFECT_SEPIA,
}

# pin assignment
CONF_HREF_PIN = "href_pin"
CONF_PIXEL_CLOCK_PIN = "pixel_clock_pin"
CONF_EXTERNAL_CLOCK = "external_clock"
CONF_I2C_PINS = "i2c_pins"
CONF_POWER_DOWN_PIN = "power_down_pin"
# image
CONF_JPEG_QUALITY = "jpeg_quality"
CONF_VERTICAL_FLIP = "vertical_flip"
CONF_HORIZONTAL_MIRROR = "horizontal_mirror"
CONF_SATURATION = "saturation"
CONF_SPECIAL_EFFECT = "special_effect"
# exposure
CONF_AEC_MODE = "aec_mode"
CONF_AEC2 = "aec2"
CONF_AE_LEVEL = "ae_level"
CONF_AEC_VALUE = "aec_value"
# gains
CONF_AGC_MODE = "agc_mode"
CONF_AGC_VALUE = "agc_value"
CONF_AGC_GAIN_CEILING = "agc_gain_ceiling"
# white balance
CONF_WB_MODE = "wb_mode"
# test pattern
CONF_TEST_PATTERN = "test_pattern"
# framerates
CONF_MAX_FRAMERATE = "max_framerate"
CONF_IDLE_FRAMERATE = "idle_framerate"

# stream trigger
CONF_ON_STREAM_START = "on_stream_start"
CONF_ON_STREAM_STOP = "on_stream_stop"
CONF_ON_IMAGE = "on_image"

camera_range_param = cv.int_range(min=-2, max=2)

CONFIG_SCHEMA = cv.ENTITY_BASE_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ESP32Camera),
        # pin assignment
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
                    cv.frequency, cv.Range(min=8e6, max=20e6)
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
        # image
        cv.Optional(CONF_RESOLUTION, default="640X480"): cv.enum(
            FRAME_SIZES, upper=True
        ),
        cv.Optional(CONF_JPEG_QUALITY, default=10): cv.int_range(min=6, max=63),
        cv.Optional(CONF_CONTRAST, default=0): camera_range_param,
        cv.Optional(CONF_BRIGHTNESS, default=0): camera_range_param,
        cv.Optional(CONF_SATURATION, default=0): camera_range_param,
        cv.Optional(CONF_VERTICAL_FLIP, default=True): cv.boolean,
        cv.Optional(CONF_HORIZONTAL_MIRROR, default=True): cv.boolean,
        cv.Optional(CONF_SPECIAL_EFFECT, default="NONE"): cv.enum(
            ENUM_SPECIAL_EFFECT, upper=True
        ),
        # exposure
        cv.Optional(CONF_AGC_MODE, default="AUTO"): cv.enum(
            ENUM_GAIN_CONTROL_MODE, upper=True
        ),
        cv.Optional(CONF_AEC2, default=False): cv.boolean,
        cv.Optional(CONF_AE_LEVEL, default=0): camera_range_param,
        cv.Optional(CONF_AEC_VALUE, default=300): cv.int_range(min=0, max=1200),
        # gains
        cv.Optional(CONF_AEC_MODE, default="AUTO"): cv.enum(
            ENUM_GAIN_CONTROL_MODE, upper=True
        ),
        cv.Optional(CONF_AGC_VALUE, default=0): cv.int_range(min=0, max=30),
        cv.Optional(CONF_AGC_GAIN_CEILING, default="2X"): cv.enum(
            ENUM_GAIN_CEILING, upper=True
        ),
        # white balance
        cv.Optional(CONF_WB_MODE, default="AUTO"): cv.enum(ENUM_WB_MODE, upper=True),
        # test pattern
        cv.Optional(CONF_TEST_PATTERN, default=False): cv.boolean,
        # framerates
        cv.Optional(CONF_MAX_FRAMERATE, default="10 fps"): cv.All(
            cv.framerate, cv.Range(min=0, min_included=False, max=60)
        ),
        cv.Optional(CONF_IDLE_FRAMERATE, default="0.1 fps"): cv.All(
            cv.framerate, cv.Range(min=0, max=1)
        ),
        cv.Optional(CONF_ON_STREAM_START): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    ESP32CameraStreamStartTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_STREAM_STOP): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(
                    ESP32CameraStreamStopTrigger
                ),
            }
        ),
        cv.Optional(CONF_ON_IMAGE): automation.validate_automation(
            {
                cv.GenerateID(CONF_TRIGGER_ID): cv.declare_id(ESP32CameraImageTrigger),
            }
        ),
    }
).extend(cv.COMPONENT_SCHEMA)

SETTERS = {
    # pin assignment
    CONF_DATA_PINS: "set_data_pins",
    CONF_VSYNC_PIN: "set_vsync_pin",
    CONF_HREF_PIN: "set_href_pin",
    CONF_PIXEL_CLOCK_PIN: "set_pixel_clock_pin",
    CONF_RESET_PIN: "set_reset_pin",
    CONF_POWER_DOWN_PIN: "set_power_down_pin",
    # image
    CONF_JPEG_QUALITY: "set_jpeg_quality",
    CONF_VERTICAL_FLIP: "set_vertical_flip",
    CONF_HORIZONTAL_MIRROR: "set_horizontal_mirror",
    CONF_CONTRAST: "set_contrast",
    CONF_BRIGHTNESS: "set_brightness",
    CONF_SATURATION: "set_saturation",
    CONF_SPECIAL_EFFECT: "set_special_effect",
    # exposure
    CONF_AEC_MODE: "set_aec_mode",
    CONF_AEC2: "set_aec2",
    CONF_AE_LEVEL: "set_ae_level",
    CONF_AEC_VALUE: "set_aec_value",
    # gains
    CONF_AGC_MODE: "set_agc_mode",
    CONF_AGC_VALUE: "set_agc_value",
    CONF_AGC_GAIN_CEILING: "set_agc_gain_ceiling",
    # white balance
    CONF_WB_MODE: "set_wb_mode",
    # test pattern
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

    for conf in config.get(CONF_ON_STREAM_START, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_STREAM_STOP, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(trigger, [], conf)

    for conf in config.get(CONF_ON_IMAGE, []):
        trigger = cg.new_Pvariable(conf[CONF_TRIGGER_ID], var)
        await automation.build_automation(
            trigger, [(ESP32CameraImageData, "image")], conf
        )
