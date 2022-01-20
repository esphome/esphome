import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import binary_sensor
from esphome.const import (
    CONF_PIN,
    CONF_THRESHOLD,
    CONF_ID,
)
from esphome.components.esp32 import gpio
from . import esp32_touch_ns, ESP32TouchComponent
from esphome.components.esp32 import get_esp32_variant
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32S2,
)

DEPENDENCIES = ["esp32_touch", "esp32"]

CONF_ESP32_TOUCH_ID = "esp32_touch_id"
CONF_WAKEUP_THRESHOLD = "wakeup_threshold"

TOUCH_PADS = {
    VARIANT_ESP32: {
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
    },
    VARIANT_ESP32S2: {
        1: cg.global_ns.TOUCH_PAD_NUM0,
        2: cg.global_ns.TOUCH_PAD_NUM1,
        3: cg.global_ns.TOUCH_PAD_NUM2,
        4: cg.global_ns.TOUCH_PAD_NUM3,
        5: cg.global_ns.TOUCH_PAD_NUM4,
        6: cg.global_ns.TOUCH_PAD_NUM5,
        7: cg.global_ns.TOUCH_PAD_NUM6,
        8: cg.global_ns.TOUCH_PAD_NUM7,
        9: cg.global_ns.TOUCH_PAD_NUM8,
        10: cg.global_ns.TOUCH_PAD_NUM9,
        11: cg.global_ns.TOUCH_PAD_NUM10,
        12: cg.global_ns.TOUCH_PAD_NUM11,
        13: cg.global_ns.TOUCH_PAD_NUM12,
        14: cg.global_ns.TOUCH_PAD_NUM13,
    },
}


def validate_touch_pad(value):
    value = gpio.validate_gpio_pin(value)
    variant = get_esp32_variant()
    if value not in TOUCH_PADS[variant]:
        raise cv.Invalid(f"{variant} doesn't support ADC on pin {value}")
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
        cv.Optional(CONF_WAKEUP_THRESHOLD, default=0): cv.uint16_t,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ESP32_TOUCH_ID])
    variant = get_esp32_variant()
    var = cg.new_Pvariable(
        config[CONF_ID],
        TOUCH_PADS[variant][config[CONF_PIN]],
        config[CONF_THRESHOLD],
        config[CONF_WAKEUP_THRESHOLD],
    )
    await binary_sensor.register_binary_sensor(var, config)
    cg.add(hub.register_touch_pad(var))
