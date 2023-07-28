import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import light
from esphome.const import (
    CONF_MAX_REFRESH_RATE,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_RGB_ORDER,
)

CODEOWNERS = ["@dentra"]
DEPENDENCIES = ["esp32"]

esp32_spi_led_strip_ns = cg.esphome_ns.namespace("esp32_spi_led_strip")
LedStripSpi = esp32_spi_led_strip_ns.class_("LedStripSpi", light.AddressableLight)
RGBOrder = esp32_spi_led_strip_ns.enum("RGBOrder")

RGB_ORDERS = {
    "RGB": RGBOrder.ORDER_RGB,
    "RBG": RGBOrder.ORDER_RBG,
    "GRB": RGBOrder.ORDER_GRB,
    "GBR": RGBOrder.ORDER_GBR,
    "BGR": RGBOrder.ORDER_BGR,
    "BRG": RGBOrder.ORDER_BRG,
}

CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(LedStripSpi),
            cv.Required(CONF_DATA_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CLOCK_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Optional(CONF_RGB_ORDER, default="BGR"): cv.enum(RGB_ORDERS, upper=True),
            cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
        }
    ),
    cv.only_on_esp32,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_data_pin(config[CONF_DATA_PIN]))
    cg.add(var.set_clock_pin(config[CONF_CLOCK_PIN]))
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_rgb_order(config[CONF_RGB_ORDER]))

    if CONF_MAX_REFRESH_RATE in config:
        cg.add(var.set_max_refresh_rate(config[CONF_MAX_REFRESH_RATE]))
