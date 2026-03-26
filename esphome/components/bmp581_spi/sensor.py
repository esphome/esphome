import logging

import esphome.codegen as cg
from esphome.components import spi
from esphome.components.spi import CONF_SPI_MODE
import esphome.config_validation as cv

from ..bmp581_base import CONFIG_SCHEMA_BASE, to_code_base

AUTO_LOAD = ["bmp581_base"]
CODEOWNERS = ["@kahrendt", "@danielkent-net"]
DEPENDENCIES = ["spi"]

_LOGGER = logging.getLogger(__name__)

VALID_SPI_MODES = {
    0: "MODE0",
    "0": "MODE0",
    "MODE0": "MODE0",
    3: "MODE3",
    "3": "MODE3",
    "MODE3": "MODE3",
}

bmp581_ns = cg.esphome_ns.namespace("bmp581_spi")
BMP581SPIComponent = bmp581_ns.class_(
    "BMP581SPIComponent", cg.PollingComponent, spi.SPIDevice
)


def check_spi_mode(config):
    spi_mode = config.get(CONF_SPI_MODE)
    if spi_mode not in VALID_SPI_MODES:
        raise cv.Invalid("BMP581 only supports SPI mode 3")
    return config


CONFIG_SCHEMA = cv.All(
    CONFIG_SCHEMA_BASE.extend(spi.spi_device_schema(default_mode="mode3")).extend(
        {cv.GenerateID(): cv.declare_id(BMP581SPIComponent)}
    ),
    check_spi_mode,
)


async def to_code(config):
    var = await to_code_base(config)
    await spi.register_spi_device(var, config)
