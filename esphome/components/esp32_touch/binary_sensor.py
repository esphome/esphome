import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import CORE
from esphome.components import binary_sensor
from esphome.const import (
    CONF_PIN,
    CONF_THRESHOLD,
    CONF_ID,
)
from esphome.components.esp32 import gpio
from esphome.components.esp32.const import (
    KEY_ESP32,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
)
from . import esp32_touch_ns, ESP32TouchComponent

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
        1: cg.global_ns.TOUCH_PAD_NUM1,
        2: cg.global_ns.TOUCH_PAD_NUM2,
        3: cg.global_ns.TOUCH_PAD_NUM3,
        4: cg.global_ns.TOUCH_PAD_NUM4,
        5: cg.global_ns.TOUCH_PAD_NUM5,
        6: cg.global_ns.TOUCH_PAD_NUM6,
        7: cg.global_ns.TOUCH_PAD_NUM7,
        8: cg.global_ns.TOUCH_PAD_NUM8,
        9: cg.global_ns.TOUCH_PAD_NUM9,
        10: cg.global_ns.TOUCH_PAD_NUM10,
        11: cg.global_ns.TOUCH_PAD_NUM11,
        12: cg.global_ns.TOUCH_PAD_NUM12,
        13: cg.global_ns.TOUCH_PAD_NUM13,
        14: cg.global_ns.TOUCH_PAD_NUM14,
    },
    VARIANT_ESP32S3: {
        1: cg.global_ns.TOUCH_PAD_NUM1,
        2: cg.global_ns.TOUCH_PAD_NUM2,
        3: cg.global_ns.TOUCH_PAD_NUM3,
        4: cg.global_ns.TOUCH_PAD_NUM4,
        5: cg.global_ns.TOUCH_PAD_NUM5,
        6: cg.global_ns.TOUCH_PAD_NUM6,
        7: cg.global_ns.TOUCH_PAD_NUM7,
        8: cg.global_ns.TOUCH_PAD_NUM8,
        9: cg.global_ns.TOUCH_PAD_NUM9,
        10: cg.global_ns.TOUCH_PAD_NUM10,
        11: cg.global_ns.TOUCH_PAD_NUM11,
        12: cg.global_ns.TOUCH_PAD_NUM12,
        13: cg.global_ns.TOUCH_PAD_NUM13,
        14: cg.global_ns.TOUCH_PAD_NUM14,
    },
}


def validate_touch_pad(value):
    value = gpio.validate_gpio_pin(value)
    variant = CORE.data[KEY_ESP32][KEY_VARIANT]
    if variant not in TOUCH_PADS:
        raise cv.Invalid(f"ESP32 variant {variant} does not support touch pads.")

    pads = TOUCH_PADS[variant]
    if value not in pads:
        raise cv.Invalid(f"Pin {value} does not support touch pads.")
    return cv.enum(pads)(value)


ESP32TouchBinarySensor = esp32_touch_ns.class_(
    "ESP32TouchBinarySensor", binary_sensor.BinarySensor
)

CONFIG_SCHEMA = binary_sensor.binary_sensor_schema(ESP32TouchBinarySensor).extend(
    {
        cv.GenerateID(CONF_ESP32_TOUCH_ID): cv.use_id(ESP32TouchComponent),
        cv.Required(CONF_PIN): validate_touch_pad,
        cv.Required(CONF_THRESHOLD): cv.uint16_t,
        cv.Optional(CONF_WAKEUP_THRESHOLD, default=0): cv.uint16_t,
    }
)


async def to_code(config):
    hub = await cg.get_variable(config[CONF_ESP32_TOUCH_ID])
    var = cg.new_Pvariable(
        config[CONF_ID],
        config[CONF_PIN],
        config[CONF_THRESHOLD],
        config[CONF_WAKEUP_THRESHOLD],
    )
    await binary_sensor.register_binary_sensor(var, config)
    cg.add(hub.register_touch_pad(var))
