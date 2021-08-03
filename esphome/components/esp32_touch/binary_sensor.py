import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_NAME,
    CONF_PIN,
    CONF_THRESHOLD,
    ESP_PLATFORM_ESP32,
    CONF_ID,
)
from esphome.pins import validate_gpio_pin
from . import esp32_touch_ns, ESP32TouchComponent

ESP_PLATFORMS = [ESP_PLATFORM_ESP32]
DEPENDENCIES = ["esp32_touch"]

CONF_ESP32_TOUCH_ID = "esp32_touch_id"

TOUCH_PADS = {
    4: cg.global_ns.TOUCH_PAD_NUM0,
    0: cg.global_ns.TOUCH_PAD_NUM1,
    2: cg.global_ns.TOUCH_PAD_NUM2,
    15: cg.global_ns.TOUCH_PAD_NUM3,
    13: cg.global_ns.TOUCH_PAD_NUM4,
    12: cg.global_ns.TOUCH_PAD_NUM5,
    14: cg.global_ns.TOUCH_PAD_NUM6,
    27: cg.global_ns.TOUCH_PAD_NUM7,
    33: cg.global_ns.TOUCH_PAD_NUM8,
    32: cg.global_ns.TOUCH_PAD_NUM9,
}


def validate_touch_pad(value):
    value = validate_gpio_pin(value)
    if value not in TOUCH_PADS:
        raise cv.Invalid(f"Pin {value} does not support touch pads.")
    return value


ESP32TouchBinarySensor = esp32_touch_ns.class_(
    "ESP32TouchBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = binary_sensor.BINARY_SENSOR_SCHEMA.extend(
    {
        cv.GenerateID(): cv.declare_id(ESP32TouchBinarySensor),
        cv.GenerateID(CONF_ESP32_TOUCH_ID): cv.use_id(ESP32TouchComponent),
        cv.Required(CONF_PIN): validate_touch_pad,
        cv.Required(CONF_THRESHOLD): cv.uint16_t,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ESP32_TOUCH_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_NAME],
        TOUCH_PADS[config[CONF_PIN]],
        config[CONF_THRESHOLD],
    )
    await binary_sensor.register_binary_sensor(var, config)
    cg.add(hub.register_touch_pad(var))
