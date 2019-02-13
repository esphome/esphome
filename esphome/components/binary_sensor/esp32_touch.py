import voluptuous as vol

from esphome.components import binary_sensor
from esphome.components.esp32_touch import ESP32TouchComponent
import esphome.config_validation as cv
from esphome.const import CONF_NAME, CONF_PIN, CONF_THRESHOLD, ESP_PLATFORM_ESP32
from esphome.cpp_generator import get_variable
from esphome.cpp_types import global_ns
from esphome.pins import validate_gpio_pin

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

DEPENDENCIES = ['esp32_touch']

CONF_ESP32_TOUCH_ID = 'esp32_touch_id'

TOUCH_PADS = {
    4: global_ns.TOUCH_PAD_NUM0,
    0: global_ns.TOUCH_PAD_NUM1,
    2: global_ns.TOUCH_PAD_NUM2,
    15: global_ns.TOUCH_PAD_NUM3,
    13: global_ns.TOUCH_PAD_NUM4,
    12: global_ns.TOUCH_PAD_NUM5,
    14: global_ns.TOUCH_PAD_NUM6,
    27: global_ns.TOUCH_PAD_NUM7,
    33: global_ns.TOUCH_PAD_NUM8,
    32: global_ns.TOUCH_PAD_NUM9,
}


def validate_touch_pad(value):
    value = validate_gpio_pin(value)
    if value not in TOUCH_PADS:
        raise vol.Invalid("Pin {} does not support touch pads.".format(value))
    return value


ESP32TouchBinarySensor = binary_sensor.binary_sensor_ns.class_('ESP32TouchBinarySensor',
                                                               binary_sensor.BinarySensor)

PLATFORM_SCHEMA = cv.nameable(binary_sensor.BINARY_SENSOR_PLATFORM_SCHEMA.extend({
    cv.GenerateID(): cv.declare_variable_id(ESP32TouchBinarySensor),
    vol.Required(CONF_PIN): validate_touch_pad,
    vol.Required(CONF_THRESHOLD): cv.uint16_t,
    cv.GenerateID(CONF_ESP32_TOUCH_ID): cv.use_variable_id(ESP32TouchComponent),
}))


def to_code(config):
    hub = None
    for hub in get_variable(config[CONF_ESP32_TOUCH_ID]):
        yield
    touch_pad = TOUCH_PADS[config[CONF_PIN]]
    rhs = hub.make_touch_pad(config[CONF_NAME], touch_pad, config[CONF_THRESHOLD])
    binary_sensor.register_binary_sensor(rhs, config)


BUILD_FLAGS = '-DUSE_ESP32_TOUCH_BINARY_SENSOR'


def to_hass_config(data, config):
    return binary_sensor.core_to_hass_config(data, config)
