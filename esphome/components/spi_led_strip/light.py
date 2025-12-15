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

Protocols = spi_led_strip_ns.enum("Protocols")

PROTOCOLS = {
    "DOTSTAR": Protocols.DOTSTAR,
    "RAW": Protocols.RAW,
}

CONF_CHANNEL_MAP = "channel_map"
CONF_MIN_MIREDS = "min_mireds"
CONF_MAX_MIREDS = "max_mireds"

VALID_CHANNELS = ["R", "G", "B", "W"]


def validate_channel_map(value):
    """Validate channel_map string and ensure only valid tokens are used."""
    for token in value.split(","):
        if token not in VALID_CHANNELS:
            raise cv.Invalid(
                f"Invalid token '{token}' in channel_map. "
                f"Valid tokens are: {', '.join(VALID_CHANNELS)}"
            )

    return value


def check_deprecated_settings(config):
    if CONF_PROTOCOL not in config:
        _LOGGER.warning(
            f"Not setting a protocol via '{CONF_PROTOCOL}' will be deprecated in a future version."
        )
        config[CONF_PROTOCOL] = "DOTSTAR"
    if CONF_CHANNEL_MAP not in config:
        _LOGGER.warning(
            f"Not setting a channel map via '{CONF_CHANNEL_MAP}' will be deprecated in a future version."
        )
        config[CONF_CHANNEL_MAP] = "B,G,R"
    return config


CONFIG_SCHEMA = cv.All(
    light.ADDRESSABLE_LIGHT_SCHEMA.extend(
        {
            cv.GenerateID(CONF_OUTPUT_ID): cv.declare_id(SpiLedStrip),
            cv.Optional(CONF_NUM_LEDS, default=1): cv.positive_not_null_int,
            cv.Optional(CONF_PROTOCOL): cv.enum(PROTOCOLS, upper=True),
            cv.Optional(CONF_CHANNEL_MAP): validate_channel_map,
            cv.Optional(CONF_MIN_MIREDS, default=154.0): cv.positive_float,
            cv.Optional(CONF_MAX_MIREDS, default=500.0): cv.positive_float,
        }
    ).extend(spi.spi_device_schema(False, "1MHz")),
    check_deprecated_settings,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_OUTPUT_ID])
    cg.add(var.set_num_leds(config[CONF_NUM_LEDS]))
    cg.add(var.set_protocol(PROTOCOLS[config[CONF_PROTOCOL]]))
    cg.add(var.set_channel_map(config[CONF_CHANNEL_MAP]))
    cg.add(var.set_min_mireds(config[CONF_MIN_MIREDS]))
    cg.add(var.set_max_mireds(config[CONF_MAX_MIREDS]))
    await light.register_light(var, config)
    await spi.register_spi_device(var, config)
    await cg.register_component(var, config)
