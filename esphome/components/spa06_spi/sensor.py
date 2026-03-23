import logging

import esphome.codegen as cg
from esphome.components import spi
from esphome.components.spi import CONF_SPI_MODE
import esphome.config_validation as cv

from ..spa06_base import CONFIG_SCHEMA_BASE, to_code_base

AUTO_LOAD = ["spa06_base"]
CODEOWNERS = ["@danielkent-net"]
DEPENDENCIES = ["spi"]

spa06_ns = cg.esphome_ns.namespace("spa06_spi")
SPA06SPIComponent = spa06_ns.class_(
    "SPA06SPIComponent", cg.PollingComponent, spi.SPIDevice
)

_LOGGER = logging.getLogger(__name__)

VALID_SPI_MODES = {3: "MODE3", "3": "MODE3", "MODE3": "MODE3"}


def check_spi_mode(config):
    spi_mode = config.get(CONF_SPI_MODE)
    if spi_mode not in VALID_SPI_MODES:
        raise cv.Invalid("SPA06 only supports SPI mode 3")
    return config


CONFIG_SCHEMA = cv.All(
    CONFIG_SCHEMA_BASE.extend(spi.spi_device_schema(default_mode="mode3")).extend(
        {cv.GenerateID(): cv.declare_id(SPA06SPIComponent)}
    ),
    check_spi_mode,
)


async def to_code(config):
    var = await to_code_base(config)
    await spi.register_spi_device(var, config)
