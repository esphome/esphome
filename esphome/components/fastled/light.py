import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import light
from esphome.const import (
    CONF_CHIPSET,
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_DATA_RATE,
    CONF_PIN,
    CONF_RGB_ORDER,
    CONF_OUTPUT_ID,
    CONF_NUM_LEDS,
    CONF_MAX_REFRESH_RATE,
)

CODEOWNERS = ["@OttoWinter", "@NielsNL68"]

fastled_base_ns = cg.esphome_ns.namespace("fastled")
FastLEDLightOutput = fastled_base_ns.class_(
    "FastLEDLightOutput", light.AddressableLight
)

RGB_ORDERS = [
    "RGB",
    "RBG",
    "GRB",
    "GBR",
    "BRG",
    "BGR",
]

SPI_CHIPSETS = [
    "LPD6803",
    "LPD8806",
    "WS2801",
    "WS2803",
    "SM16716",
    "P9813",
    "APA102",
    "SK9822",
    "DOTSTAR",
]

CLOCKLESS_CHIPSETS = [
    "NEOPIXEL",
    "TM1829",
    "TM1809",
    "TM1804",
    "TM1803",
    "UCS1903",
    "UCS1903B",
    "UCS1904",
    "UCS2903",
    "WS2812",
    "WS2852",
    "WS2812B",
    "SK6812",
    "SK6822",
    "APA106",
    "PL9823",
    "WS2811",
    "WS2813",
    "APA104",
    "WS2811_400",
    "GW6205",
    "GW6205_400",
    "LPD1886",
    "LPD1886_8BIT",
    "SM16703",
]

CHIPSETS = CLOCKLESS_CHIPSETS + SPI_CHIPSETS


def _validate(config):
    if config[CONF_CHIPSET] == "NEOPIXEL" and CONF_RGB_ORDER in config:
        raise cv.Invalid("NEOPIXEL doesn't support RGB order")
    if config[CONF_CHIPSET] in SPI_CHIPSETS and config[CONF_CLOCK_PIN] == -1:
        raise cv.Invalid("The clock_pin is required for SPI devices.")

    return config


def validate_gpio_output_pin_number(value):
    if value == -1:
        return value
    return pins.internal_gpio_output_pin_number(value)


BASE_SCHEMA = light.ADDRESSABLE_LIGHT_SCHEMA.extend(
    {
        cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(FastLEDLightOutput),
        cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
        cv.Optional(CONF_RGB_ORDER): cv.one_of(*RGB_ORDERS, upper=True),
        cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
    }
).extend(cv.COMPONENT_SCHEMA)


CONFIG_SCHEMA = cv.All(
    BASE_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(FastLEDLightOutput),
            cv.Required(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Optional(CONF_RGB_ORDER): cv.one_of(*RGB_ORDERS, upper=True),
            cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
            cv.Optional(CONF_DATA_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_PIN): cv.Invalid("This pin is renamed to 'data_pin'"),
            cv.Optional(CONF_CLOCK_PIN, default=-1): validate_gpio_output_pin_number,
            cv.Optional(CONF_DATA_RATE): cv.frequency,
        }
    ),
    _validate,
    cv.only_on_arduino,
)


async def new_fastled_light(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await cg.register_component(var, config)

    if CONF_MAX_REFRESH_RATE in config:
        cg.add(var.set_max_refresh_rate(config[CONF_MAX_REFRESH_RATE]))

    await light.register_light(var, config)
    cg.add_library("fastled/FastLED", "3.6.0")
    return var


async def to_code(config):
    var = await new_fastled_light(config)

    rgb_order = cg.RawExpression(
        config[CONF_RGB_ORDER] if CONF_RGB_ORDER in config else None
    )
    rgb_order = None
    if CONF_RGB_ORDER in config:
        rgb_order = cg.RawExpression(config[CONF_RGB_ORDER])

    if config[CONF_CHIPSET] in SPI_CHIPSETS:
        if CONF_DATA_RATE in config:
            data_rate_khz = int(config[CONF_DATA_RATE] / 1000)
            if data_rate_khz < 1000:
                data_rate = cg.RawExpression(f"DATA_RATE_KHZ({data_rate_khz})")
            else:
                data_rate_mhz = int(data_rate_khz / 1000)
                data_rate = cg.RawExpression(f"DATA_RATE_MHZ({data_rate_mhz})")
        else:
            data_rate = None
        template_args = cg.TemplateArguments(
            cg.RawExpression(config[CONF_CHIPSET]),
            config[CONF_DATA_PIN],
            config[CONF_CLOCK_PIN],
            rgb_order,
            data_rate,
        )
    else:
        template_args = cg.TemplateArguments(
            cg.RawExpression(config[CONF_CHIPSET]), config[CONF_DATA_PIN], rgb_order
        )
    cg.add(var.add_leds(template_args, config[CONF_NUM_LEDS]))
