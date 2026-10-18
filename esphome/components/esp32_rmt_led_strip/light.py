from dataclasses import dataclass

from esphome import pins
import esphome.codegen as cg
from esphome.components import esp32, esp32_rmt, light
from esphome.components.const import CONF_CHANNEL_COLORS, CONF_IS_WRGB, CONF_USE_PSRAM
from esphome.components.esp32 import include_builtin_idf_component
import esphome.config_validation as cv
from esphome.const import (
    CONF_CHIPSET,
    CONF_INVERTED,
    CONF_IS_RGBW,
    CONF_MAX_REFRESH_RATE,
    CONF_NUM_LEDS,
    CONF_NUMBER,
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_RGB_ORDER,
    CONF_RMT_SYMBOLS,
    CONF_USE_DMA,
)
from esphome.types import ConfigType

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32"]

esp32_rmt_led_strip_ns = cg.esphome_ns.namespace("esp32_rmt_led_strip")
ESP32RMTLEDStripLightOutput = esp32_rmt_led_strip_ns.class_(
    "ESP32RMTLEDStripLightOutput", light.AddressableLight
)


@dataclass
class LEDStripTimings:
    bit0_high: int
    bit0_low: int
    bit1_high: int
    bit1_low: int
    reset_high: int
    reset_low: int


CHIPSETS = {
    "WS2811": LEDStripTimings(300, 1090, 1090, 320, 0, 300000),
    "WS2812": LEDStripTimings(400, 1000, 1000, 400, 0, 0),
    "SK6812": LEDStripTimings(300, 900, 600, 600, 0, 0),
    "APA106": LEDStripTimings(350, 1360, 1360, 350, 0, 0),
    "SM16703": LEDStripTimings(300, 900, 900, 300, 0, 0),
}

CONF_BIT0_HIGH = "bit0_high"
CONF_BIT0_LOW = "bit0_low"
CONF_BIT1_HIGH = "bit1_high"
CONF_BIT1_LOW = "bit1_low"
CONF_RESET_HIGH = "reset_high"
CONF_RESET_LOW = "reset_low"


CONFIG_SCHEMA = cv.All(
    esp32.only_on_variant(
        unsupported=list(esp32_rmt.VARIANTS_NO_RMT),
        msg_prefix="ESP32 RMT LED strip",
    ),
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(ESP32RMTLEDStripLightOutput),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_schema,
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Optional(CONF_CHANNEL_COLORS): light.validate_channel_colors,
            # Deprecated in favour of CONF_CHANNEL_COLORS, remove in 2027.3.0
            cv.Optional(CONF_RGB_ORDER): cv.one_of(*light.RGB_ORDERS, upper=True),
            cv.Optional(CONF_IS_RGBW): cv.boolean,
            cv.Optional(CONF_IS_WRGB): cv.boolean,
            cv.SplitDefault(
                CONF_RMT_SYMBOLS,
                esp32=192,
                esp32_c3=96,
                esp32_c5=96,
                esp32_c6=96,
                esp32_h2=96,
                esp32_p4=192,
                esp32_s2=192,
                esp32_s3=192,
            ): cv.int_range(min=2),
            cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
            cv.Optional(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
            cv.Optional(CONF_USE_DMA): cv.All(
                esp32.only_on_variant(
                    supported=[esp32.VARIANT_ESP32P4, esp32.VARIANT_ESP32S3]
                ),
                cv.boolean,
            ),
            cv.Optional(CONF_USE_PSRAM, default=True): cv.boolean,
            cv.Inclusive(
                CONF_BIT0_HIGH,
                "custom",
            ): cv.positive_time_period_nanoseconds,
            cv.Inclusive(
                CONF_BIT0_LOW,
                "custom",
            ): cv.positive_time_period_nanoseconds,
            cv.Inclusive(
                CONF_BIT1_HIGH,
                "custom",
            ): cv.positive_time_period_nanoseconds,
            cv.Inclusive(
                CONF_BIT1_LOW,
                "custom",
            ): cv.positive_time_period_nanoseconds,
            cv.Optional(
                CONF_RESET_HIGH,
                default="0 us",
            ): cv.positive_time_period_nanoseconds,
            cv.Optional(
                CONF_RESET_LOW,
                default="0 us",
            ): cv.positive_time_period_nanoseconds,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.has_exactly_one_key(CONF_CHIPSET, CONF_BIT0_HIGH),
    light.migrate_channel_colors(
        removed_in="2027.3.0", component="esp32_rmt_led_strip"
    ),
)


async def to_code(config: ConfigType) -> None:
    # Re-enable ESP-IDF's RMT driver (excluded by default to save compile time)
    include_builtin_idf_component("esp_driver_rmt")

    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    await light.register_light(var, config)
    await cg.register_component(var, config)

    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_pin(config[CONF_PIN][CONF_NUMBER]))
    if config[CONF_PIN][CONF_INVERTED]:
        cg.add(var.set_inverted(True))

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
                chipset.reset_high,
                chipset.reset_low,
            )
        )
    else:
        cg.add(
            var.set_led_params(
                config[CONF_BIT0_HIGH],
                config[CONF_BIT0_LOW],
                config[CONF_BIT1_HIGH],
                config[CONF_BIT1_LOW],
                config[CONF_RESET_HIGH],
                config[CONF_RESET_LOW],
            )
        )

    cg.add(
        var.set_channel_colors(light.channel_colors_struct(config[CONF_CHANNEL_COLORS]))
    )
    cg.add(var.set_use_psram(config[CONF_USE_PSRAM]))
    cg.add(var.set_rmt_symbols(config[CONF_RMT_SYMBOLS]))
    if CONF_USE_DMA in config:
        cg.add(var.set_use_dma(config[CONF_USE_DMA]))
