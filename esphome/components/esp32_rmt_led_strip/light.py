from dataclasses import dataclass

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import light
from esphome.const import (
    CONF_MAX_REFRESH_RATE,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_CHIPSET,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32"]

esp32_rmt_led_strip_ns = cg.esphome_ns.namespace("esp32_rmt_led_strip")
ESP32RMTLEDStripLightOutput = esp32_rmt_led_strip_ns.class_(
    "ESP32RMTLEDStripLightOutput", light.AddressableLight
)

rgb_order_t = esp32_rmt_led_strip_ns.enum("rgb_order_t")

RGB_ORDERS = {
    "RGB": rgb_order_t.ORDER_RGB,
    "RBG": rgb_order_t.ORDER_RBG,
    "GRB": rgb_order_t.ORDER_GRB,
    "GBR": rgb_order_t.ORDER_GBR,
    "BGR": rgb_order_t.ORDER_BGR,
    "BRG": rgb_order_t.ORDER_BRG,
}


@dataclass
class LEDStripTimings:
    bit0_high: int
    bit0_low: int
    bit1_high: int
    bit1_low: int


CHIPSETS = {
    "WS2812": LEDStripTimings(400, 1000, 1000, 400),
    "SK6812": LEDStripTimings(300, 900, 600, 600),
    "APA106": LEDStripTimings(350, 1360, 1360, 350),
    "SM16703": LEDStripTimings(300, 900, 1360, 350),
}


CONF_IS_RGBW = "is_rgbw"
CONF_RGB_ORDER = "rgb_order"
CONF_BIT0_HIGH = "bit0_high"
CONF_BIT0_LOW = "bit0_low"
CONF_BIT1_HIGH = "bit1_high"
CONF_BIT1_LOW = "bit1_low"

CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(ESP32RMTLEDStripLightOutput),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
            cv.Optional(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
            cv.Optional(CONF_IS_RGBW, default=False): cv.boolean,
            cv.Inclusive(
                CONF_BIT0_HIGH,
                "custom",
            ): cv.positive_time_period_microseconds,
            cv.Inclusive(
                CONF_BIT0_LOW,
                "custom",
            ): cv.positive_time_period_microseconds,
            cv.Inclusive(
                CONF_BIT1_HIGH,
                "custom",
            ): cv.positive_time_period_microseconds,
            cv.Inclusive(
                CONF_BIT1_LOW,
                "custom",
            ): cv.positive_time_period_microseconds,
            cv.Required(CONF_RGB_ORDER): cv.enum(RGB_ORDERS, upper=True),
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
    else:
        cg.add(
            var.set_led_params(
                config[CONF_BIT0_HIGH],
                config[CONF_BIT0_LOW],
                config[CONF_BIT1_HIGH],
                config[CONF_BIT1_LOW],
            )
        )

    cg.add(var.set_rgb_order(config[CONF_RGB_ORDER]))

    cg.add(var.set_is_rgbw(config[CONF_IS_RGBW]))
