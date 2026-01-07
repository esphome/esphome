import logging

import esphome.codegen as cg
from esphome.components import light, spi
import esphome.config_validation as cv
from esphome.const import CONF_NUM_LEDS, CONF_OUTPUT_ID, CONF_PROTOCOL

_LOGGER = logging.getLogger(__name__)

spi_led_strip_ns = cg.esphome_ns.namespace("spi_led_strip")
SpiLedStrip = spi_led_strip_ns.class_(
    "SpiLedStrip", light.AddressableLight, spi.SPIDevice
)
Protocol = spi_led_strip_ns.enum("Protocol")
PROTOCOL = {
    "APA102": Protocol.APA102,
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
        config[CONF_PROTOCOL] = "APA102"

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

    return config


def get_light_traits(value):
    if "W" in value:
        return light.ColorMode.RGB_WHITE

    return light.ColorMode.RGB


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SpiLedStrip),
            cv.Optional(CONF_PROTOCOL): cv.enum(PROTOCOL, upper=True),
            cv.Optional(CONF_CHANNEL_MAP): [cv.string],
            cv.Optional(CONF_NUM_LEDS, default=1): cv.positive_not_null_int,
        }
    ).extend(spi.spi_device_schema(False, "1MHz")),
    check_deprecated_settings,
    validate_settings,
)


async def to_code(config):
    channels = list(light.CHANNEL_NAME.keys())
    channel_array = [-1] * len(light.CHANNEL_NAME)

    for order, channel in enumerate(config[CONF_CHANNEL_MAP]):
        channel_index = channels.index(channel)
        channel_array[channel_index] = order

    channel_map = light.ChannelMap(
        cg.ArrayInitializer(*channel_array),
        len(config[CONF_CHANNEL_MAP]),
        ",".join(config[CONF_CHANNEL_MAP]),
        get_light_traits(config[CONF_CHANNEL_MAP]),
    )

    var = cg.new_Pvariable(
        config[CONF_OUTPUT_ID],
        PROTOCOL[config[CONF_PROTOCOL]],
        channel_map,
        config[CONF_NUM_LEDS],
    )

    await light.register_light(var, config)
    await spi.register_spi_device(var, config)
    await cg.register_component(var, config)
