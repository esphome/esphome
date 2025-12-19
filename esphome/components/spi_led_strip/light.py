import logging

import esphome.codegen as cg
from esphome.components import light, spi
import esphome.config_validation as cv
from esphome.const import (
    CONF_COLD_WHITE_COLOR_TEMPERATURE,
    CONF_NUM_LEDS,
    CONF_OUTPUT_ID,
    CONF_PROTOCOL,
    CONF_WARM_WHITE_COLOR_TEMPERATURE,
)

_LOGGER = logging.getLogger(__name__)

CHANNEL_NAME = {
    "R": light.ChannelName.R,
    "G": light.ChannelName.G,
    "B": light.ChannelName.B,
    "W": light.ChannelName.W,
    "CW": light.ChannelName.CW,
    "WW": light.ChannelName.WW,
}

spi_led_strip_ns = cg.esphome_ns.namespace("spi_led_strip")
SpiLedStrip = spi_led_strip_ns.class_(
    "SpiLedStrip", light.AddressableLight, spi.SPIDevice
)
Protocol = spi_led_strip_ns.enum("Protocol")
PROTOCOL = {
    "DOTSTAR": Protocol.DOTSTAR,
    "RAW": Protocol.RAW,
}

CONF_CHANNEL_MAP = "channel_map"

VALID_CHANNELS = ["R", "G", "B", "W"]


def check_deprecated_settings(config):
    if CONF_PROTOCOL not in config:
        _LOGGER.warning(
            "Not setting a protocol via '%s' will be deprecated in a future version.",
            CONF_PROTOCOL,
        )
        config[CONF_PROTOCOL] = "DOTSTAR"

    if CONF_CHANNEL_MAP not in config:
        _LOGGER.warning(
            "Not setting a channel map via '%s' will be deprecated in a future version.",
            CONF_CHANNEL_MAP,
        )
        config[CONF_CHANNEL_MAP] = ["B", "G", "R"]
    return config


def validate_settings(config):
    for token in config[CONF_CHANNEL_MAP]:
        if token not in VALID_CHANNELS:
            raise cv.Invalid(
                f"Invalid token '{token}' in channel_map. "
                f"Valid tokens are: {', '.join(VALID_CHANNELS)}"
            )

    if len(set(config[CONF_CHANNEL_MAP]) & {"CW", "WW"}) == 1:
        raise cv.Invalid(
            "Channel 'CW' can only be used together with channel 'WW' (and vice versa). "
            "For single white channels use 'W' instead."
        )

    if (
        config[CONF_COLD_WHITE_COLOR_TEMPERATURE]
        < config[CONF_WARM_WHITE_COLOR_TEMPERATURE]
    ):
        raise cv.Invalid(
            f"'{CONF_COLD_WHITE_COLOR_TEMPERATURE}' must be greater than '{CONF_WARM_WHITE_COLOR_TEMPERATURE}'."
        )
    return config


def get_light_traits(value):
    if len(set(value) & {"CW", "WW"}) == 2:
        return light.ColorMode.RGB_COLD_WARM_WHITE
    if "W" in value:
        return light.ColorMode.RGB_WHITE

    return light.ColorMode.RGB


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SpiLedStrip),
            cv.Optional(CONF_PROTOCOL): cv.enum(PROTOCOL, upper=True),
            cv.Optional(CONF_CHANNEL_MAP): [cv.string],
            cv.Optional(
                CONF_COLD_WHITE_COLOR_TEMPERATURE, default=6500
            ): cv.positive_not_null_int,
            cv.Optional(
                CONF_WARM_WHITE_COLOR_TEMPERATURE, default=2700
            ): cv.positive_not_null_int,
            cv.Optional(CONF_NUM_LEDS, default=1): cv.positive_not_null_int,
        }
    ).extend(spi.spi_device_schema(False, "1MHz")),
    check_deprecated_settings,
    validate_settings,
)


async def to_code(config):
    channel_names = [CHANNEL_NAME[token] for token in config[CONF_CHANNEL_MAP]]
    ordered_channel_names = cg.std_vector.template(light.ChannelName)(
        cg.ArrayInitializer(*channel_names)
    )

    channel_map = light.ChannelMap(
        ordered_channel_names,
        ",".join(config[CONF_CHANNEL_MAP]),
        get_light_traits(config[CONF_CHANNEL_MAP]),
    )

    var = cg.new_Pvariable(
        config[CONF_OUTPUT_ID],
        PROTOCOL[config[CONF_PROTOCOL]],
        channel_map,
        config[CONF_NUM_LEDS],
    )
    cg.add(
        var.set_cold_white_color_temperature(config[CONF_COLD_WHITE_COLOR_TEMPERATURE])
    )
    cg.add(
        var.set_warm_white_color_temperature(config[CONF_WARM_WHITE_COLOR_TEMPERATURE])
    )
    await light.register_light(var, config)
    await spi.register_spi_device(var, config)
    await cg.register_component(var, config)
