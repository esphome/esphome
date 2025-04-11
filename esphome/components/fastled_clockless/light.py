from esphome import pins
import esphome.codegen as cg
from esphome.components import fastled_base
import esphome.config_validation as cv
from esphome.const import CONF_CHIPSET, CONF_NUM_LEDS, CONF_PIN, CONF_RGB_ORDER

AUTO_LOAD = ["fastled_base"]

CHIPSETS = [
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


def _validate(value):
    if value[CONF_CHIPSET] == "NEOPIXEL" and CONF_RGB_ORDER in value:
        raise cv.Invalid("NEOPIXEL doesn't support RGB order")
    return value


CONFIG_SCHEMA = cv.All(
    fastled_base.BASE_SCHEMA.extend(
        {
            cv.Required(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
        }
    ),
    _validate,
    cv.require_framework_version(
        esp8266_arduino=cv.Version(2, 7, 4),
        esp32_arduino=cv.Version(99, 0, 0),
        max_version=True,
        extra_message="Please see note on documentation for FastLED",
    ),
)


async def to_code(config):
    var = await fastled_base.new_fastled_light(config)

    rgb_order = None
    if CONF_RGB_ORDER in config:
        rgb_order = cg.RawExpression(config[CONF_RGB_ORDER])
    template_args = cg.TemplateArguments(
        cg.RawExpression(config[CONF_CHIPSET]), config[CONF_PIN], rgb_order
    )
    cg.add(var.add_leds(template_args, config[CONF_NUM_LEDS]))
