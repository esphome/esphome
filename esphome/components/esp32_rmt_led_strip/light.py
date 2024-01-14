from dataclasses import dataclass, field
from typing import Optional

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
from esphome.components import esp32, light
from esphome.components.light.types import (
    ColorMode,
    COLOR_MODES,
)
from esphome.const import (
    CONF_CHIPSET,
    CONF_INVERTED,
    CONF_MAX_REFRESH_RATE,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_PIN,
    CONF_RGB_ORDER,
)

CODEOWNERS = ["@jesserockz"]
DEPENDENCIES = ["esp32"]

esp32_rmt_led_strip_ns = cg.esphome_ns.namespace("esp32_rmt_led_strip")
ESP32RMTLEDStripLightOutput = esp32_rmt_led_strip_ns.class_(
    "ESP32RMTLEDStripLightOutput", light.AddressableLight
)

rmt_channel_t = cg.global_ns.enum("rmt_channel_t")
rmt_item32_t = cg.global_ns.struct("rmt_item32_t")

RGBOrder = esp32_rmt_led_strip_ns.enum("RGBOrder")

RGB_ORDERS = {
    "RGB": RGBOrder.ORDER_RGB,
    "RBG": RGBOrder.ORDER_RBG,
    "GRB": RGBOrder.ORDER_GRB,
    "GBR": RGBOrder.ORDER_GBR,
    "BGR": RGBOrder.ORDER_BGR,
    "BRG": RGBOrder.ORDER_BRG,
}

Encoding = esp32_rmt_led_strip_ns.enum("Encoding")

ENCODINGS = {
    "PULSE_LENGTH": Encoding.ENCODING_PULSE_LENGTH,
    "PULSE_DISTANCE": Encoding.ENCODING_PULSE_DISTANCE,
    "BI_PHASE": Encoding.ENCODING_BI_PHASE,
}


@dataclass
class LEDStripChipConfigs:
    bit0_high: int
    bit0_low: int
    bit1_high: int
    bit1_low: int
    encoding: Encoding = Encoding.ENCODING_PULSE_LENGTH
    rmt_generator: Optional[str] = None
    sync_start: int = 0
    intermission: int = 0
    bits_per_command: int = 0
    color_modes: list[ColorMode] = field(default_factory=lambda: [ColorMode.RGB])
    allow_partial_updates: bool = False
    internal_is_rgbw: bool = False


CHIPSETS = {
    "WS2812": LEDStripChipConfigs(400, 1000, 1000, 400),
    "SK6812": LEDStripChipConfigs(300, 900, 600, 600),
    "APA106": LEDStripChipConfigs(350, 1360, 1360, 350),
    "SM16703": LEDStripChipConfigs(300, 900, 1360, 350),
    "RDS-RGBW-02": LEDStripChipConfigs(
        22_000,
        22_000,
        22_000,
        61_000,
        encoding=Encoding.ENCODING_PULSE_DISTANCE,
        rmt_generator="esp32_rmt_led_strip::rds_rgbw_02_rmt_generator",
        sync_start=77_000,
        intermission=5_000,
        bits_per_command=32,
        color_modes=[ColorMode.RGB, ColorMode.WHITE],
        allow_partial_updates=True,
        internal_is_rgbw=True,
    ),
}


CONF_IS_RGBW = "is_rgbw"
CONF_SUPPORTED_COLOR_MODES = "supported_color_modes"
CONF_SYNC_START = "sync_start"
CONF_ENCODING = "encoding"
CONF_INTERMISSION = "intermission"
CONF_BIT0_HIGH = "bit0_high"
CONF_BIT0_LOW = "bit0_low"
CONF_BIT1_HIGH = "bit1_high"
CONF_BIT1_LOW = "bit1_low"
CONF_RMT_CHANNEL = "rmt_channel"
CONF_RMT_GENERATOR = "rmt_generator"
CONF_ALLOW_PARTIAL_UPDATES = "allow_partial_updates"

RMT_CHANNELS = {
    esp32.const.VARIANT_ESP32: [0, 1, 2, 3, 4, 5, 6, 7],
    esp32.const.VARIANT_ESP32S2: [0, 1, 2, 3],
    esp32.const.VARIANT_ESP32S3: [0, 1, 2, 3],
    esp32.const.VARIANT_ESP32C3: [0, 1],
    esp32.const.VARIANT_ESP32C6: [0, 1],
    esp32.const.VARIANT_ESP32H2: [0, 1],
}


def _validate_rmt_channel(value):
    variant = esp32.get_esp32_variant()
    if variant not in RMT_CHANNELS:
        raise cv.Invalid(f"ESP32 variant {variant} does not support RMT.")
    if value not in RMT_CHANNELS[variant]:
        raise cv.Invalid(
            f"RMT channel {value} is not supported for ESP32 variant {variant}."
        )
    return value


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(ESP32RMTLEDStripLightOutput),
            cv.Required(CONF_PIN): pins.internal_gpio_output_pin_number,
            cv.Required(CONF_NUM_LEDS): cv.positive_not_null_int,
            cv.Required(CONF_RGB_ORDER): cv.enum(RGB_ORDERS, upper=True),
            cv.Required(CONF_RMT_CHANNEL): _validate_rmt_channel,
            cv.Optional(CONF_RMT_GENERATOR): cv.lambda_,
            cv.Optional(CONF_MAX_REFRESH_RATE): cv.positive_time_period_microseconds,
            cv.Optional(CONF_CHIPSET): cv.one_of(*CHIPSETS, upper=True),
            cv.Optional(CONF_IS_RGBW): cv.boolean,
            cv.Optional(CONF_ALLOW_PARTIAL_UPDATES): cv.boolean,
            cv.Optional(CONF_SUPPORTED_COLOR_MODES): cv.ensure_list(
                cv.enum(COLOR_MODES, upper=True)
            ),
            cv.Optional(CONF_SYNC_START): cv.positive_time_period_nanoseconds,
            cv.Optional(CONF_INTERMISSION): cv.positive_time_period_microseconds,
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
            cv.Optional(CONF_INVERTED, default=False): cv.boolean,
            cv.Optional(CONF_ENCODING, default="PULSE_LENGTH"): cv.enum(
                ENCODINGS, upper=True
            ),
        }
    ),
    cv.has_exactly_one_key(CONF_CHIPSET, CONF_BIT0_HIGH),
    cv.has_at_most_one_key(CONF_CHIPSET, CONF_SYNC_START),
    cv.has_at_most_one_key(CONF_CHIPSET, CONF_INTERMISSION),
    cv.has_at_most_one_key(CONF_SUPPORTED_COLOR_MODES, CONF_IS_RGBW),
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
        cg.add(var.set_encoding(chipset.encoding))
        cg.add(var.set_allow_partial_updates(chipset.allow_partial_updates))

        if chipset.sync_start > 0:
            cg.add(var.set_sync_start(chipset.sync_start))

        if chipset.intermission > 0:
            cg.add(var.set_intermission(chipset.intermission))

        if chipset.bits_per_command > 0:
            cg.add(var.set_bits_per_command(chipset.bits_per_command))

        # allow users to override and shoot themselves in the foot
        if CONF_SUPPORTED_COLOR_MODES not in config and CONF_IS_RGBW not in config:
            cg.add(var.set_supported_color_modes(chipset.color_modes))

        if chipset.internal_is_rgbw:
            cg.add(var.set_internal_is_rgbw(True))

        if CONF_RMT_GENERATOR not in config and chipset.rmt_generator is not None:
            cg.add(var.set_rmt_generator(cg.RawExpression(chipset.rmt_generator)))
    else:
        cg.add(
            var.set_led_params(
                config[CONF_BIT0_HIGH],
                config[CONF_BIT0_LOW],
                config[CONF_BIT1_HIGH],
                config[CONF_BIT1_LOW],
            )
        )

        if CONF_ALLOW_PARTIAL_UPDATES in config:
            cg.add(var.set_allow_partial_updates(config[CONF_ALLOW_PARTIAL_UPDATES]))

        if CONF_ENCODING in config:
            cg.add(var.set_encoding(config[CONF_ENCODING]))

        if CONF_SYNC_START in config:
            cg.add(var.set_sync_start(config[CONF_SYNC_START]))

        if CONF_INTERMISSION in config:
            cg.add(var.set_intermission(config[CONF_INTERMISSION]))

    cg.add(var.set_rgb_order(config[CONF_RGB_ORDER]))

    if CONF_SUPPORTED_COLOR_MODES in config:
        cg.add(var.set_supported_color_modes(config[CONF_SUPPORTED_COLOR_MODES]))
    elif CONF_IS_RGBW in config:
        cg.add(var.set_is_rgbw(config[CONF_IS_RGBW]))

    cg.add(var.set_is_inverted(config[CONF_INVERTED]))

    cg.add(
        var.set_rmt_channel(
            getattr(rmt_channel_t, f"RMT_CHANNEL_{config[CONF_RMT_CHANNEL]}")
        )
    )

    if CONF_RMT_GENERATOR in config:
        cg.add(
            var.set_rmt_generator(
                await cg.process_lambda(
                    config[CONF_RMT_GENERATOR],
                    [
                        (
                            ESP32RMTLEDStripLightOutput.operator("const").operator(
                                "ref"
                            ),
                            "light",
                        ),
                        (cg.int_, "index"),
                        (cg.uint8.operator("const").operator("ptr"), "src_buf"),
                        (rmt_item32_t.operator("ptr"), "dest_buf"),
                        (light.LightState.operator("ptr"), "state"),
                    ],
                )
            )
        )
