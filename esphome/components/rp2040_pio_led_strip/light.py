import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import light, rp2040
from esphome.const import (
    CONF_CHIPSET,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_RGB_ORDER,
)

CONF_PIO = "pio"

CODEOWNERS = ["@Papa-DMan"]
DEPENDENCIES = ["rp2040"]

rp2040_pio_led_strip_ns = cg.esphome_ns.namespace("rp2040_pio_led_strip")
RP2040PIOLEDStripLightOutput = rp2040_pio_led_strip_ns.class_(
    "RP2040PIOLEDStripLightOutput", light.AddressableLight
)

RGBOrder = rp2040_pio_led_strip_ns.enum("RGBOrder")

Chipsets = rp2040_pio_led_strip_ns.enum("Chipset")

CHIPSETS = {
    "WS2812": Chipsets.WS2812,
    "WS2812B": Chipsets.WS2812B,
    "SK6812": Chipsets.SK6812,
    "SM16703": Chipsets.SM16703,
}

RGB_ORDERS = {
    "RGB": RGBOrder.ORDER_RGB,
    "RBG": RGBOrder.ORDER_RBG,
    "GRB": RGBOrder.ORDER_GRB,
    "GBR": RGBOrder.ORDER_GBR,
    "BGR": RGBOrder.ORDER_BGR,
    "BRG": RGBOrder.ORDER_BRG,
}

CONF_IS_RGBW = "is_rgbw"

PIO_VALUES = {rp2040.const: [0, 1]}


def _validate_pio_value(value):
    value = cv.int_(value)
    if value < 0 or value > 1:
        raise cv.Invalid("Value must be between 0 and 1")
    return value


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(RP2040PIOLEDStripLightOutput),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Required(CONF_RGB_ORDER): cv.enum(RGB_ORDERS, upper=True),
            cv.Required(CONF_PIO): _validate_pio_value,
            cv.Required(CONF_CHIPSET, default="WS2812"): cv.enum(CHIPSETS, upper=True),
            cv.Optional(CONF_IS_RGBW, default=False): cv.boolean,
        }
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_pin(config[CONF_PIN]))

    cg.add(var.set_chipset(CHIPSETS[config[CONF_CHIPSET]]))

    cg.add(var.set_rgb_order(config[CONF_RGB_ORDER]))
    cg.add(var.set_is_rgbw(config[CONF_IS_RGBW]))

    cg.add(var.set_pio(config[CONF_PIO]))
