"""Shared model for boards using the E Ink EL133UF1 panel (EPaperEL133UF1)."""

from typing import Any

from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_CS_PIN, CONF_DIMENSIONS, CONF_INIT_SEQUENCE
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import EpaperModel, ultrachip_resolution

CONF_CS1_PIN = "cs1_pin"
# Widest row the driver can stage: 4 * EPaperEL133UF1::MAX_HALF_ROW_BYTES (epaper_spi_el133uf1.h),
# since each chip receives a quarter of the width in bytes per row.
MAX_WIDTH = 1600

# Chip masks for init sequence entries, the same values as CHIP_* in epaper_spi_el133uf1.h
PRIMARY = 1
BOTH = 3

# Data of the CMD66 (0xF0) command, the same as EPaperEL133UF1::CMD66_V
CMD66_DATA = (0x49, 0x55, 0x13, 0x5D, 0x05, 0x10)


class EL133UF1Model(EpaperModel):
    """A panel whose two controller chips each drive half of every row, one chip select each.

    Init sequence entries are (command, chip mask, *data), or delay(ms) from esphome.components.mipi.
    """

    source_files = ("epaper_spi_4bpp.cpp", "epaper_spi_el133uf1.cpp")

    # The driver drives CS and CS1 directly for the dual-CS protocol.
    manages_cs = True

    def resolution(self, config: ConfigType) -> tuple[int, ...]:
        """The resolution (TRES) entry: each chip is set up with the full width and half the height."""
        width, height = self.get_dimensions(config)
        command, *data = ultrachip_resolution(width, height // 2)
        return (command, BOTH, *data)

    def validate_config(self, config: ConfigType) -> ConfigType:
        if CONF_INIT_SEQUENCE in config:
            raise cv.Invalid(
                f"{self.name} does not support a custom '{CONF_INIT_SEQUENCE}'",
                path=[CONF_INIT_SEQUENCE],
            )
        width, height = self.get_dimensions(config)
        if width > MAX_WIDTH:
            raise cv.Invalid(
                f"{self.name} supports a width of at most {MAX_WIDTH}",
                path=[CONF_DIMENSIONS],
            )
        # Each chip gets half a row, packed 2 pixels per byte, and partial windows round
        # columns to groups of 4 and rows to pairs.
        if width % 4:
            raise cv.Invalid(
                f"{self.name} requires a width that is a multiple of 4",
                path=[CONF_DIMENSIONS],
            )
        if height % 2:
            raise cv.Invalid(
                f"{self.name} requires an even height", path=[CONF_DIMENSIONS]
            )
        return config

    def get_config_options(self) -> dict[Any, Any]:
        # fallback=None makes CS1 required unless the model provides a default.
        return {
            self.option(CONF_CS1_PIN, fallback=None): pins.gpio_output_pin_schema,
        }

    async def to_code(self, var: MockObj, config: ConfigType) -> ConfigType:
        await cg.add_pin(var.set_primary_cs_pin, config[CONF_CS_PIN])
        await cg.add_pin(var.set_secondary_cs_pin, config[CONF_CS1_PIN])
        # Remove CS and CS1 from the config so that the base class doesn't try to handle them.
        return {k: v for k, v in config.items() if k not in (CONF_CS_PIN, CONF_CS1_PIN)}
