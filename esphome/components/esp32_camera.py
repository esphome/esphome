import voluptuous as vol

from esphome import config_validation as cv, pins
from esphome.const import CONF_FREQUENCY, CONF_ID, CONF_NAME, CONF_PIN, CONF_SCL, CONF_SDA, \
    ESP_PLATFORM_ESP32
from esphome.cpp_generator import Pvariable, add
from esphome.cpp_types import App, Nameable, PollingComponent, esphome_ns

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

ESP32Camera = esphome_ns.class_('ESP32Camera', PollingComponent, Nameable)
ESP32CameraFrameSize = esphome_ns.enum('ESP32CameraFrameSize')
FRAME_SIZES = {
    '160X120': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_160X120,
    'QQVGA': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_160X120,
    '128x160': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_128X160,
    'QQVGA2': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_128X160,
    '176X144': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_176X144,
    'QCIF': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_176X144,
    '240X176': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_240X176,
    'HQVGA': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_240X176,
    '320X240': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_320X240,
    'QVGA': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_320X240,
    '400X296': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_400X296,
    'CIF': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_400X296,
    '640X480': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_640X480,
    'VGA': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_640X480,
    '800X600': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_800X600,
    'SVGA': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_800X600,
    '1024X768': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1024X768,
    'XGA': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1024X768,
    '1280x1024': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1280X1024,
    'SXGA': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1280X1024,
    '1600X1200': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1600X1200,
    'UXGA': ESP32CameraFrameSize.ESP32_CAMERA_SIZE_1600X1200,
}

CONF_DATA_PINS = 'data_pins'
CONF_VSYNC_PIN = 'vsync_pin'
CONF_HREF_PIN = 'href_pin'
CONF_PIXEL_CLOCK_PIN = 'pixel_clock_pin'
CONF_EXTERNAL_CLOCK = 'external_clock'
CONF_I2C_PINS = 'i2c_pins'
CONF_RESET_PIN = 'reset_pin'
CONF_POWER_DOWN_PIN = 'power_down_pin'

CONF_MAX_FRAMERATE = 'max_framerate'
CONF_IDLE_FRAMERATE = 'idle_framerate'
CONF_RESOLUTION = 'resolution'
CONF_JPEG_QUALITY = 'jpeg_quality'
CONF_VERTICAL_FLIP = 'vertical_flip'
CONF_HORIZONTAL_MIRROR = 'horizontal_mirror'
CONF_CONTRAST = 'contrast'
CONF_BRIGHTNESS = 'brightness'
CONF_SATURATION = 'saturation'
CONF_TEST_PATTERN = 'test_pattern'

camera_range_param = vol.All(cv.int_, vol.Range(min=-2, max=2))

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_variable_id(ESP32Camera),
    vol.Required(CONF_NAME): cv.string,
    vol.Required(CONF_DATA_PINS): vol.All([pins.input_pin], vol.Length(min=8, max=8)),
    vol.Required(CONF_VSYNC_PIN): pins.input_pin,
    vol.Required(CONF_HREF_PIN): pins.input_pin,
    vol.Required(CONF_PIXEL_CLOCK_PIN): pins.input_pin,
    vol.Required(CONF_EXTERNAL_CLOCK): cv.Schema({
        vol.Required(CONF_PIN): pins.output_pin,
        vol.Optional(CONF_FREQUENCY, default='20MHz'): vol.All(cv.frequency, vol.In([20e6, 10e6])),
    }),
    vol.Required(CONF_I2C_PINS): cv.Schema({
        vol.Required(CONF_SDA): pins.output_pin,
        vol.Required(CONF_SCL): pins.output_pin,
    }),
    vol.Optional(CONF_RESET_PIN): pins.output_pin,
    vol.Optional(CONF_POWER_DOWN_PIN): pins.output_pin,

    vol.Optional(CONF_MAX_FRAMERATE, default='10 fps'): vol.All(cv.framerate,
                                                                vol.Range(min=0, min_included=False,
                                                                          max=60)),
    vol.Optional(CONF_IDLE_FRAMERATE, default='0.1 fps'): vol.All(cv.framerate,
                                                                  vol.Range(min=0, max=1)),
    vol.Optional(CONF_RESOLUTION, default='640X480'): cv.one_of(*FRAME_SIZES, upper=True),
    vol.Optional(CONF_JPEG_QUALITY, default=10): vol.All(cv.int_, vol.Range(min=10, max=63)),
    vol.Optional(CONF_CONTRAST, default=0): camera_range_param,
    vol.Optional(CONF_BRIGHTNESS, default=0): camera_range_param,
    vol.Optional(CONF_SATURATION, default=0): camera_range_param,
    vol.Optional(CONF_VERTICAL_FLIP, default=True): cv.boolean,
    vol.Optional(CONF_HORIZONTAL_MIRROR, default=True): cv.boolean,
    vol.Optional(CONF_TEST_PATTERN, default=False): cv.boolean,
}).extend(cv.COMPONENT_SCHEMA.schema)

SETTERS = {
    CONF_DATA_PINS: 'set_data_pins',
    CONF_VSYNC_PIN: 'set_vsync_pin',
    CONF_HREF_PIN: 'set_href_pin',
    CONF_PIXEL_CLOCK_PIN: 'set_pixel_clock_pin',
    CONF_RESET_PIN: 'set_reset_pin',
    CONF_POWER_DOWN_PIN: 'set_power_down_pin',
    CONF_JPEG_QUALITY: 'set_jpeg_quality',
    CONF_VERTICAL_FLIP: 'set_vertical_flip',
    CONF_HORIZONTAL_MIRROR: 'set_horizontal_mirror',
    CONF_CONTRAST: 'set_contrast',
    CONF_BRIGHTNESS: 'set_brightness',
    CONF_SATURATION: 'set_saturation',
    CONF_TEST_PATTERN: 'set_test_pattern',
}


def to_code(config):
    rhs = App.register_component(ESP32Camera.new(config[CONF_NAME]))
    cam = Pvariable(config[CONF_ID], rhs)

    for key, setter in SETTERS.items():
        if key in config:
            add(getattr(cam, setter)(config[key]))

    extclk = config[CONF_EXTERNAL_CLOCK]
    add(cam.set_external_clock(extclk[CONF_PIN], extclk[CONF_FREQUENCY]))
    i2c_pins = config[CONF_I2C_PINS]
    add(cam.set_i2c_pins(i2c_pins[CONF_SDA], i2c_pins[CONF_SCL]))
    add(cam.set_max_update_interval(1000 / config[CONF_MAX_FRAMERATE]))
    if config[CONF_IDLE_FRAMERATE] == 0:
        add(cam.set_idle_update_interval(0))
    else:
        add(cam.set_idle_update_interval(1000 / config[CONF_IDLE_FRAMERATE]))
    add(cam.set_frame_size(FRAME_SIZES[config[CONF_RESOLUTION]]))


BUILD_FLAGS = ['-DUSE_ESP32_CAMERA', '-DBOARD_HAS_PSRAM']
