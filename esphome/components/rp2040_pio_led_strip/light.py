from dataclasses import dataclass

import esphome.codegen as cg
import esphome.config_validation as cv

from esphome import pins
from esphome.components import light, rp2040
from esphome.const import (
    CONF_CHIPSET,
    CONF_MAX_REFRESH_RATE,
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

RGB_ORDERS = {
    "RGB": RGBOrder.ORDER_RGB,
    "RBG": RGBOrder.ORDER_RBG,
    "GRB": RGBOrder.ORDER_GRB,
    "GBR": RGBOrder.ORDER_GBR,
    "BGR": RGBOrder.ORDER_BGR,
    "BRG": RGBOrder.ORDER_BRG,
}


@dataclass
class LEDStripTimings:
    bit0_high: int
    bit0_low: int
    bit1_high: int
    bit1_low: int


CHIPSETS = {
    "WS2812": LEDStripTimings(12, 12, 24, 12),
    "WS2812B": LEDStripTimings(12, 12, 24, 12),
    "SK6812": LEDStripTimings(12, 12, 24, 12),
    "APA106": LEDStripTimings(12, 12, 24, 12),
    "SM16703": LEDStripTimings(12, 12, 24, 12),
}

CONF_IS_RGBW = "is_rgbw"
CONF_BIT0_HIGH = "bit0_high"
CONF_BIT0_LOW = "bit0_low"
CONF_BIT1_HIGH = "bit1_high"
CONF_BIT1_LOW = "bit1_low"

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
            cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
            cv.Optional(CONF_CHIPSET, default="WS2812"): cv.enum(CHIPSETS, upper=True),
            cv.Optional(CONF_IS_RGBW, default=False): cv.boolean,
        }
    ),
    cv.has_exactly_one_key(CONF_CHIPSET, CONF_BIT0_HIGH),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_pin(config[CONF_PIN]))

    if CONF_MAX_REFRESH_RATE in config:
        cg.add(var.set_max_refresh_rate(config[CONF_MAX_REFRESH_RATE]))

    if CONF_CHIPSET in config:
        chipset = CHIPSETS[config[CONF_CHIPSET]]
        cg.add(
            var.set_led_params(
                chipset.bit0_high,
                chipset.bit0_low,
                chipset.bit1_high,
                chipset.bit1_low,
            )
        )

    cg.add(var.set_rgb_order(config[CONF_RGB_ORDER]))
    cg.add(var.set_is_rgbw(config[CONF_IS_RGBW]))

    cg.add(var.set_pio(config[CONF_PIO]))
