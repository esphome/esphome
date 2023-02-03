import esphome.codegen as cg
from esphome.components import fastled_bus
from esphome.components.fastled_bus import CONF_BUS, CONF_CHIP_CHANNELS
import esphome.config_validation as cv
from esphome import pins
from esphome.components import fastled_base, fastled_bus_spi
from esphome.const import (
    CONF_CHIPSET,
    CONF_CLOCK_PIN,
    CONF_DATA_PIN,
    CONF_DATA_RATE,
    CONF_ID,
    CONF_NUM_CHIPS,
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


def _validate(value):
    # if not CONF_BUS in value:
    # fastled_component = get_component("fastled_bus_clockless")
    # busConfig = cv.Schema(fastled_bus_clockless.CONFIG_SCHEMA)
    # print("Adding bus config")
    # value[CONF_BUS] = f"{value[CONF_ID]}_spi_bus"
    return value


CONFIG_SCHEMA = cv.All(
    fastled_base.BASE_SCHEMA.extend(
        {
            cv.Required(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
            cv.Required(CONF_DATA_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_CLOCK_PIN): pins.internal_gpio_output_pin_number,
            cv.Optional(CONF_DATA_RATE): cv.frequency,
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
            fastled_bus.rgb_order(config),
            config[CONF_DATA_PIN],
            config[CONF_CLOCK_PIN],
            data_rate,
        )
        bus = cg.RawExpression(
            f"""[]() {{
    auto bus = new fastled_bus::FastLEDBus(3, {config[CONF_NUM_LEDS]});
    bus->set_controller(fastled_bus::CLEDControllerFactory::create{template_args}());
    return bus;
}}()"""
        )
    else:
        print("BUS--->" + str(config[CONF_BUS]))
        bus = await cg.get_variable(config[CONF_BUS])

    cg.add(var.set_bus(bus))
    # cg.add(var.add_leds(template_args, config[CONF_NUM_LEDS]))
