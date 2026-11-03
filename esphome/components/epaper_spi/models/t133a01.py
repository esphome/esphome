"""T133A01-based e-paper displays.

The T133A01 is a 6-color e-paper controller IC that drives large panels
(1200x1600 portrait). It uses a dual-CS SPI architecture where CS
controls one half of the pixel data and CS1 controls the other half,
as well as panel-level commands (power on, refresh, power off).

Supported models:
- Seeed-reTerminal-E1004: 1200x1600 pixels, 6-color (T133A01 panel)
- 13.3in-Spectra-E6: Good Display GDEP133C02 (Waveshare 13.3" HAT+ (E)), no D/C line
"""

from typing import Any

from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_BUSY_PIN,
    CONF_CS_PIN,
    CONF_DC_PIN,
    CONF_DIMENSIONS,
    CONF_INVERTED,
    CONF_NUMBER,
)
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import EpaperModel, ultrachip_resolution

CONF_CS1_PIN = "cs1_pin"

# Placeholder for the resolution command, filled in from the configured dimensions
_RESOLUTION = (0x61,)

# Must match T133A01_MAX_WIDTH in epaper_spi_t133a01.cpp, which sizes the row buffer
MAX_WIDTH = 1600


def _busy_pin_inverted_by_default(value: Any) -> Any:
    """The T133A01 busy line is active low, so invert the pin unless the config says otherwise."""
    if not isinstance(value, dict):
        value = {CONF_NUMBER: value}
    return {CONF_INVERTED: True, **value}


class T133A01Model(EpaperModel):
    """EpaperModel subclass for T133A01-based 6-color e-paper displays."""

    source_files = ("epaper_spi_t133a01.cpp",)

    # The driver drives CS and CS1 directly for the dual-CS protocol.
    manages_cs = True

    def __init__(self, name, class_name="EPaperT133A01", **defaults):
        super().__init__(name, class_name, **defaults)

    def option(self, name, fallback=cv.UNDEFINED) -> cv.Optional | cv.Required:
        # Some panels have no D/C line and frame commands with the chip selects alone
        if name == CONF_DC_PIN:
            fallback = cv.UNDEFINED
        # The driver drives CS itself, so it is required unless the model provides it
        elif name == CONF_CS_PIN:
            fallback = None
        return super().option(name, fallback)

    def get_config_options(self) -> dict:
        # CS1 is the second chip-select required by the dual-CS architecture.
        # fallback=None makes it required unless the model provides a default.
        return {
            self.option(CONF_CS1_PIN, fallback=None): pins.gpio_output_pin_schema,
            # Replaces the shared busy_pin schema
            self.option(CONF_BUSY_PIN): cv.All(
                _busy_pin_inverted_by_default, pins.gpio_input_pin_schema
            ),
        }

    def validate_config(self, config: ConfigType) -> ConfigType:
        width, _ = self.get_dimensions(config)
        if width > MAX_WIDTH:
            raise cv.Invalid(
                f"{self.name} supports a width of at most {MAX_WIDTH}",
                path=[CONF_DIMENSIONS],
            )
        return config

    async def to_code(self, var: MockObj, config: dict) -> dict:
        cs = await cg.gpio_pin_expression(config[CONF_CS_PIN])
        cs1 = await cg.gpio_pin_expression(config[CONF_CS1_PIN])
        cg.add(var.set_cs_pins(cs, cs1))
        # Remove CS and CS1 from the config so that the base class doesn't try to handle them.
        return {k: v for k, v in config.items() if k not in (CONF_CS_PIN, CONF_CS1_PIN)}

    def get_init_sequence(self, config: dict):
        # Each controller drives half of every row, so the resolution is the width and half the height
        width, height = self.get_dimensions(config)
        return tuple(
            ultrachip_resolution(width, height // 2) if seq == _RESOLUTION else seq
            for seq in self.initsequence
        )


# The driver sends 0x74, 0xA5, 0x01, 0xB6, 0x06, 0xB7, 0x05, 0xB0 and 0xB1 to the CS controller only,
# every other command to both.
t133a01_base = T133A01Model(
    "t133a01",
    minimum_update_interval="30s",
    width_multiple=4,
    data_rate="10MHz",
    initsequence=(
        (0x74, 0x00, 0x0C, 0x0C, 0xD9, 0xDD, 0xDD, 0x15, 0x15, 0x55),
        (0xF0, 0x49, 0x55, 0x13, 0x5D, 0x05, 0x10),
        (0x00, 0xDF, 0x69),
        (0xA5, 0x44, 0x54, 0x00),
        (0x50, 0x37),
        (0x60, 0x03, 0x03),
        (0x86, 0x10),
        (0xE3, 0x22),
        _RESOLUTION,
        (0x01, 0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38),
        (0xB6, 0x07),
        (0x06, 0xE0, 0x20),
        (0xB7, 0x01),
        (0x05, 0xE0, 0x20),
        (0xB0, 0x01),
        (0xB1, 0x02),
    ),
)

# Seeed reTerminal E1004 - 13.3" 6-color e-paper (1200x1600, T133A01)
# Portrait orientation (1200 wide × 1600 tall), matching the Arduino
# Setup523 defines TFT_WIDTH=1200, TFT_HEIGHT=1600.
# CS and CS1 each receive half of each row's pixel data
# (300 bytes = 600 pixels per controller, for all 1600 rows).
Seeed_reTerminal_E1004 = t133a01_base.extend(
    "Seeed-reTerminal-E1004",
    width=1200,
    height=1600,
    cs_pin=10,
    cs1_pin=2,
    dc_pin=11,
    reset_pin=38,
    busy_pin={
        "number": 13,
        "inverted": True,
        "mode": {"input": True},
    },
    enable_pin=12,
)

# Good Display GDEP133C02 (Waveshare 13.3" HAT+ (E)): same controller pair, no D/C line, active low busy
t133a01_base.extend(
    "13.3in-Spectra-E6",
    width=1200,
    height=1600,
    data_rate="2MHz",
    initsequence=(
        (0x74, 0xC0, 0x1C, 0x1C, 0xCC, 0xCC, 0xCC, 0x15, 0x15, 0x55),
        (0xF0, 0x49, 0x55, 0x13, 0x5D, 0x05, 0x10),
        (0x00, 0xDF, 0x69),
        (0x50, 0xF7),
        (0x60, 0x03, 0x03),
        (0x86, 0x10),
        (0xE3, 0x22),
        _RESOLUTION,
        (0x01, 0x0F, 0x00, 0x28, 0x2C, 0x28, 0x38),
        (0xB6, 0x07),
        (0x06, 0xE8, 0x28),
        (0xB7, 0x01),
        (0x05, 0xE8, 0x28),
        (0xB0, 0x01),
        (0xB1, 0x02),
    ),
)
