from dataclasses import dataclass

from esphome import pins
import esphome.codegen as cg
from esphome.components import libretiny, light
from esphome.components.const import CONF_CHANNEL_COLORS, CONF_IS_WRGB
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHIPSET,
    CONF_IS_RGBW,
    CONF_MAX_REFRESH_RATE,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_RGB_ORDER,
)
from esphome.types import ConfigType

CODEOWNERS = ["@Mat931"]
DEPENDENCIES = ["libretiny"]

beken_spi_led_strip_ns = cg.esphome_ns.namespace("beken_spi_led_strip")
BekenSPILEDStripLightOutput = beken_spi_led_strip_ns.class_(
    "BekenSPILEDStripLightOutput", light.AddressableLight
)


@dataclass
class LEDStripTimings:
    bit0: int
    bit1: int
    spi_frequency: int


CHIPSETS = {
    "WS2812": LEDStripTimings(
        0b11100000, 0b11111100, 6666666
    ),  # Clock divider: 9, Bit time: 1350ns
    "SK6812": LEDStripTimings(
        0b11000000, 0b11111000, 7500000
    ),  # Clock divider: 8, Bit time: 1200ns
    "APA106": LEDStripTimings(
        0b11000000, 0b11111110, 5454545
    ),  # Clock divider: 11, Bit time: 1650ns
    "SM16703": LEDStripTimings(
        0b11000000, 0b11111110, 7500000
    ),  # Clock divider: 8, Bit time: 1200ns
}


SUPPORTED_PINS = {
    libretiny.const.FAMILY_BK7231N: [16],
    libretiny.const.FAMILY_BK7231T: [16],
    libretiny.const.FAMILY_BK7238: [16],
    libretiny.const.FAMILY_BK7251: [16],
}


def _validate_pin(value: int) -> int:
    family = libretiny.get_libretiny_family()
    if family not in SUPPORTED_PINS:
        raise cv.Invalid(f"Chip family {family} is not supported.")
    if value not in SUPPORTED_PINS[family]:
        supported_pin_info = ", ".join(f"{x}" for x in SUPPORTED_PINS[family])
        raise cv.Invalid(
            f"Pin {value} is not supported on the {family}. Supported pins: {supported_pin_info}"
        )
    return value


def _validate_num_leds(value: ConfigType) -> ConfigType:
    # A white channel makes each LED one byte wider, so fewer of them fit in the DMA buffer.
    max_num_leds = 123 if "W" in value[CONF_CHANNEL_COLORS] else 165  # 127 / 170
    if value[CONF_NUM_LEDS] > max_num_leds:
        raise cv.Invalid(
            f"The maximum number of LEDs for this configuration is {max_num_leds}.",
            path=CONF_NUM_LEDS,
        )
    return value


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(BekenSPILEDStripLightOutput),
            cv.Required(CONF_PIN): cv.All(
                pins.internal_gpio_output_pin_number, _validate_pin
            ),
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Optional(CONF_CHANNEL_COLORS): light.validate_channel_colors,
            # Deprecated in favour of CONF_CHANNEL_COLORS, remove in 2027.3.0
            cv.Optional(CONF_RGB_ORDER): cv.one_of(*light.RGB_ORDERS, upper=True),
            cv.Optional(CONF_IS_RGBW): cv.boolean,
            cv.Optional(CONF_IS_WRGB): cv.boolean,
            cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
            cv.Required(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
        }
    ),
    light.migrate_channel_colors(
        removed_in="2027.3.0", component="beken_spi_led_strip"
    ),
    _validate_num_leds,
)


async def to_code(config: ConfigType) -> None:
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_pin(config[CONF_PIN]))

    if CONF_MAX_REFRESH_RATE in config:
        cg.add(var.set_max_refresh_rate(config[CONF_MAX_REFRESH_RATE]))

    chipset = CHIPSETS[config[CONF_CHIPSET]]
    cg.add(
        var.set_led_params(
            chipset.bit0,
            chipset.bit1,
            chipset.spi_frequency,
        )
    )

    cg.add(
        var.set_channel_colors(light.channel_colors_struct(config[CONF_CHANNEL_COLORS]))
    )
