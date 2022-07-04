import copy
import esphome.codegen as cg
from esphome.components import fastled_bus
from esphome.components.fastled_bus import CONF_BUS, CONF_CHIP_CHANNELS, FastledBusId
import esphome.config_validation as cv
from esphome import pins
from esphome.components import fastled_base, fastled_bus_clockless
from esphome.const import CONF_CHIPSET, CONF_ID, CONF_NUM_CHIPS, CONF_NUM_LEDS, CONF_PIN, CONF_RGB_ORDER
from esphome.loader import get_component

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
        out = value.copy()
    # if not CONF_BUS in value:
    #     # fastled_component = get_component("fastled_bus_clockless")
    #     # busConfig = cv.Schema(fastled_bus_clockless.CONFIG_SCHEMA)
    #     print("Adding bus config")
    #     value[CONF_BUS] = f"{value[CONF_ID]}_clockless_bus"
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
    if not CONF_BUS in config:
        template_args = cg.TemplateArguments(
            cg.RawExpression(config[CONF_CHIPSET]),
            fastled_bus.rgb_order(config),
            config[CONF_PIN],
        )

        bus = cg.RawExpression(f'''[]() {{
    auto bus = new fastled_bus::FastLEDBus(3, {config[CONF_NUM_LEDS]});
    bus->set_controller(fastled_bus::CLEDControllerFactory::create{template_args}());
    return bus;
}}()''')
    else:
        print("BUS--->" + str(config[CONF_BUS]))
        bus = await cg.get_variable(config[CONF_BUS])


    # template_args = cg.TemplateArguments(
    #     cg.RawExpression(config[CONF_CHIPSET]), config[CONF_PIN], rgb_order
    # )
    # cg.add(var.set_bus(bus))
    # cg.add(var.add_leds(template_args, config[CONF_NUM_LEDS]))
    cg.add(var.set_bus(bus))
