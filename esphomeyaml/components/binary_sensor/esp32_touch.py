import voluptuous as vol

import esphomeyaml.config_validation as cv
from esphomeyaml.components import binary_sensor
from esphomeyaml.const import CONF_ID, CONF_NAME, CONF_PIN, CONF_THRESHOLD, ESP_PLATFORM_ESP32
from esphomeyaml.helpers import Pvariable, RawExpression, get_variable
from esphomeyaml.pins import validate_gpio_pin

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]

DEPENDENCIES = ['esp32_touch']

TOUCH_PADS = {
    4: 'TOUCH_PAD_NUM0',
    0: 'TOUCH_PAD_NUM1',
    2: 'TOUCH_PAD_NUM2',
    15: 'TOUCH_PAD_NUM3',
    13: 'TOUCH_PAD_NUM4',
    12: 'TOUCH_PAD_NUM5',
    14: 'TOUCH_PAD_NUM6',
    27: 'TOUCH_PAD_NUM7',
    33: 'TOUCH_PAD_NUM8',
    32: 'TOUCH_PAD_NUM9',
}


def validate_touch_pad(value):
    value = validate_gpio_pin(value)
    if value not in TOUCH_PADS:
        raise vol.Invalid("Pin {} does not support touch pads.".format(value))
    return value


PLATFORM_SCHEMA = binary_sensor.PLATFORM_SCHEMA.extend({
    cv.GenerateID('esp32_touch_pad'): cv.register_variable_id,
    vol.Required(CONF_PIN): validate_touch_pad,
    vol.Required(CONF_THRESHOLD): cv.uint16_t,
}).extend(binary_sensor.MQTT_BINARY_SENSOR_ID_SCHEMA.schema)


def to_code(config):
    hub = get_variable(None, type='binary_sensor::ESP32TouchComponent')
    touch_pad = RawExpression(TOUCH_PADS[config[CONF_PIN]])
    rhs = hub.make_touch_pad(config[CONF_NAME], touch_pad, config[CONF_THRESHOLD])
    device = Pvariable('ESP32TouchBinarySensor', config[CONF_ID], rhs)
    binary_sensor.register_binary_sensor(device, config)


BUILD_FLAGS = '-DUSE_ESP32_TOUCH_BINARY_SENSOR'
