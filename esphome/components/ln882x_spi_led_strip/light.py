from dataclasses import dataclass

from esphome import pins
import esphome.codegen as cg
from esphome.components import light
from esphome.components.const import CONF_CHANNEL_COLORS
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHIPSET,
    CONF_MAX_REFRESH_RATE,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_PIN,
)
import esphome.final_validate as fv
from esphome.types import ConfigType

from . import DOMAIN

CODEOWNERS = ["@sebo-b"]
DEPENDENCIES = ["ln882x"]

ln882x_spi_led_strip_ns = cg.esphome_ns.namespace("ln882x_spi_led_strip")
LN882xSPILEDStripLightOutput = ln882x_spi_led_strip_ns.class_(
    "LN882xSPILEDStripLightOutput", light.AddressableLight
)


@dataclass(frozen=True)
class LEDStripTimings:
    bit0_high: int  # ns
    bit0_low: int
    bit1_high: int
    bit1_low: int


# As esp32_rmt_led_strip.
CHIPSETS = {
    "WS2811": LEDStripTimings(300, 1090, 1090, 320),
    "WS2812": LEDStripTimings(400, 1000, 1000, 400),
    "SK6812": LEDStripTimings(300, 900, 600, 600),
    "APA106": LEDStripTimings(350, 1360, 1360, 350),
    "SM16703": LEDStripTimings(300, 900, 900, 300),
}

# SystemInit() in the SDK sets APB0 to 40 MHz; led_strip.cpp checks it at boot.
APB0_HZ = 40_000_000
# SPI0 runs at APB0 / 2^prescaler, prescaler 1 (/2) to 8 (/256).
SPI_PRESCALER_MIN = 1
SPI_PRESCALER_MAX = 8
# Each LED data bit is sent as one SPI byte, MSB first: the leading 1-bits are the high time.
SPI_BITS_PER_LED_BIT = 8
# At most this many high bits, so every LED bit ends with at least two low SPI bits.
SPI_HIGH_BITS_MAX = SPI_BITS_PER_LED_BIT - 2
# Low time after the data that latches it. Minimum 50 us for WS2811, 80 us for SK6812, 280 us for WS2812B-V5.
RESET_DURATION_NS = 300_000
# One DMA transfer moves at most this many bytes: the whole frame plus the reset gap.
DMA_MAX_TRANSFER = 0xFFFF


@dataclass(frozen=True)
class SPIEncoding:
    prescaler: int
    bit0_high_bits: int
    bit1_high_bits: int
    reset_bytes: int


def _spi_bit_ns(prescaler: int) -> int:
    return (1_000_000_000 << prescaler) // APB0_HZ


def spi_encoding(timings: LEDStripTimings) -> SPIEncoding:
    """SPI clock and bit patterns that reproduce the chipset timings."""
    # The prescaler whose 8-bit frame is closest to the bit period. Each step doubles the frame,
    # so step on while the period is at least 1.5 frames (a tie takes the longer frame).
    bit_period_ns = max(
        timings.bit0_high + timings.bit0_low, timings.bit1_high + timings.bit1_low
    )
    prescaler = SPI_PRESCALER_MIN
    while (
        prescaler < SPI_PRESCALER_MAX
        and 3 * SPI_BITS_PER_LED_BIT * _spi_bit_ns(prescaler) <= 2 * bit_period_ns
    ):
        prescaler += 1
    bit_ns = _spi_bit_ns(prescaler)

    def high_bits(duration_ns: int, max_bits: int) -> int:
        # Rounded half up, from 1 to max_bits.
        return max(1, min(max_bits, (2 * duration_ns + bit_ns) // (2 * bit_ns)))

    # Bit 0 stops one below the maximum, so bit 1 can always be longer.
    bit0_high_bits = high_bits(timings.bit0_high, SPI_HIGH_BITS_MAX - 1)
    bit1_high_bits = max(
        high_bits(timings.bit1_high, SPI_HIGH_BITS_MAX), bit0_high_bits + 1
    )
    reset_bytes = -(-RESET_DURATION_NS // (SPI_BITS_PER_LED_BIT * bit_ns))
    return SPIEncoding(prescaler, bit0_high_bits, bit1_high_bits, reset_bytes)


def _validate_num_leds(config: ConfigType) -> ConfigType:
    reset_bytes = spi_encoding(CHIPSETS[config[CONF_CHIPSET]]).reset_bytes
    bytes_per_led = len(config[CONF_CHANNEL_COLORS])
    max_num_leds = (
        (DMA_MAX_TRANSFER - reset_bytes) // SPI_BITS_PER_LED_BIT // bytes_per_led
    )
    if config[CONF_NUM_LEDS] > max_num_leds:
        raise cv.Invalid(
            f"The maximum number of LEDs for this configuration is {max_num_leds}.",
            path=[CONF_NUM_LEDS],
        )
    return config


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(LN882xSPILEDStripLightOutput),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Required(CONF_CHANNEL_COLORS): light.validate_channel_colors,
            cv.Required(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
            cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
        }
    ),
    _validate_num_leds,
)


# The strip owns SPI0 and its DMA channel, so only one can exist.
FINAL_VALIDATE_SCHEMA = fv.unique_platform_key(
    domain=light.DOMAIN,
    platform=DOMAIN,
    message=f"Only one {DOMAIN} is supported (it uses SPI0)",
)


async def to_code(config: ConfigType) -> None:
    encoding = spi_encoding(CHIPSETS[config[CONF_CHIPSET]])
    var = cg.new_Pvariable(
        config[CONF_OUTPUT_ID],
        config[CONF_PIN],
        config[CONF_NUM_LEDS],
        light.channel_colors_struct(config[CONF_CHANNEL_COLORS]),
        encoding.prescaler,
        encoding.bit0_high_bits,
        encoding.bit1_high_bits,
        encoding.reset_bytes,
    )
    await light.register_light(var, config)
    await cg.register_component(var, config)

    cg.add_optional(var.set_max_refresh_rate, config.get(CONF_MAX_REFRESH_RATE))
