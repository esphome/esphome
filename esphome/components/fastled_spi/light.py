import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import fastled_base
from esphome.const import (
    CONF_CHIPSET,
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_DATA_RATE,
    CONF_NUM_LEDS,
    CONF_RGB_ORDER,
)

AUTO_LOAD = ["fastled_base"]

CHIPSETS = [
    "LPD8806",
    "WS2801",
    "WS2803",
    "SM16716",
    "P9813",
    "APA102",
    "SK9822",
    "DOTSTAR",
]

CONFIG_SCHEMA = cv.All(
    fastled_base.BASE_SCHEMA.extend(
        {
            cv.Required(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
            cv.Required(CONF_DATA_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CLOCK_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_DATA_RATE): cv.frequency,
        }
    ),
    cv.require_framework_version(
        esp8266_arduino=cv.Version(2, 7, 4),
        esp32_arduino=cv.Version(99, 0, 0),
        max_version=True,
        extra_message="Please see note on documentation for FastLED",
    ),
)


async def to_code(config):
    var = await fastled_base.new_fastled_light(config)

    rgb_order = cg.RawExpression(
        config[CONF_RGB_ORDER] if CONF_RGB_ORDER in config else "RGB"
    )
    data_rate = None

    if CONF_DATA_RATE in config:
        data_rate_khz = int(config[CONF_DATA_RATE] / 1000)
        if data_rate_khz < 1000:
            data_rate = cg.RawExpression(f"DATA_RATE_KHZ({data_rate_khz})")
        else:
            data_rate_mhz = int(data_rate_khz / 1000)
            data_rate = cg.RawExpression(f"DATA_RATE_MHZ({data_rate_mhz})")
    template_args = cg.TemplateArguments(
        cg.RawExpression(config[CONF_CHIPSET]),
        config[CONF_DATA_PIN],
        config[CONF_CLOCK_PIN],
        rgb_order,
        data_rate,
    )
    cg.add(var.add_leds(template_args, config[CONF_NUM_LEDS]))
