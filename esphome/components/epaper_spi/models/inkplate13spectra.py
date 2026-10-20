# The panel is split into two halves, each with its own chip-select: CS (primary, left half
# of every row) and CS1 (secondary, right half). BS0/BS1 select the controllers' interface mode.
# RST and PWR_EN are driven directly by this driver (not through the generic reset_pin_ /
# enable_pins_ mechanisms) because the power-on sequence toggles them dynamically on every
# refresh cycle, not just once at setup.

from typing import Any

from esphome import pins
import esphome.codegen as cg
from esphome.components.const import CONF_RST_PIN
import esphome.config_validation as cv
from esphome.const import CONF_CS_PIN, CONF_DIMENSIONS, CONF_HEIGHT, CONF_WIDTH
from esphome.cpp_generator import MockObj
from esphome.types import ConfigType

from . import EpaperModel

CONF_CS1_PIN = "cs1_pin"
CONF_BS0_PIN = "bs0_pin"
CONF_BS1_PIN = "bs1_pin"
CONF_PWR_EN_PIN = "pwr_en_pin"


class Inkplate13SpectraModel(EpaperModel):
    # The driver drives CS and CS1 directly for the dual-chip protocol.
    manages_cs = True

    def __init__(
        self, name: str, class_name: str = "EPaperInkplate13Spectra", **defaults: Any
    ) -> None:
        super().__init__(name, class_name, **defaults)

    def get_constructor_args(self, config: ConfigType) -> tuple[bool]:
        # toggle_dc: this panel uses 3-wire SPI (BS0/BS1 select the mode), so
        # write_command_to_chip_() must not toggle the DC pin -- cmd/data is inferred by
        # byte position instead.
        return (False,)

    def validate_config(self, config: ConfigType) -> ConfigType:
        # The panel is fixed on the board and the init sequence sets its resolution, so other
        # dimensions would only mismatch the frame buffer and the panel.
        width, height = self.get_default(CONF_WIDTH), self.get_default(CONF_HEIGHT)
        if self.get_dimensions(config) != (width, height):
            raise cv.Invalid(
                f"{self.name} has a fixed {width}x{height} panel; remove 'dimensions'",
                path=[CONF_DIMENSIONS],
            )
        return config

    def get_config_options(self) -> dict[Any, Any]:
        return {
            self.option(CONF_CS1_PIN, fallback=None): pins.gpio_output_pin_schema,
            self.option(CONF_BS0_PIN, fallback=None): pins.gpio_output_pin_schema,
            self.option(CONF_BS1_PIN, fallback=None): pins.gpio_output_pin_schema,
            self.option(CONF_RST_PIN, fallback=None): pins.gpio_output_pin_schema,
            self.option(CONF_PWR_EN_PIN, fallback=None): pins.gpio_output_pin_schema,
        }

    async def to_code(self, var: MockObj, config: ConfigType) -> ConfigType:
        await cg.add_pin(var.set_primary_cs_pin, config[CONF_CS_PIN])
        await cg.add_pin(var.set_secondary_cs_pin, config[CONF_CS1_PIN])
        await cg.add_pin(var.set_bs0_pin, config[CONF_BS0_PIN])
        await cg.add_pin(var.set_bs1_pin, config[CONF_BS1_PIN])
        await cg.add_pin(var.set_rst_pin, config[CONF_RST_PIN])
        await cg.add_pin(var.set_pwr_en_pin, config[CONF_PWR_EN_PIN])
        # Remove these from the config so the base class doesn't try to handle them
        # (CS via the generic single-CS SPIDevice mechanism, RST via reset_pin_/reset()).
        return {
            k: v
            for k, v in config.items()
            if k
            not in (
                CONF_CS_PIN,
                CONF_CS1_PIN,
                CONF_BS0_PIN,
                CONF_BS1_PIN,
                CONF_RST_PIN,
                CONF_PWR_EN_PIN,
            )
        }


# Confirmed on real hardware: rotation=0 needs mirror_x + mirror_y but not swap_xy.
# initialise() sends its own hardcoded, per-command-routed register sequence instead of
# using the generic init sequence mechanism, so no initsequence is set here.
inkplate13spectra = Inkplate13SpectraModel(
    "inkplate13spectra",
    width=1200,
    height=1600,
    swap_xy=False,
    mirror_x=True,
    mirror_y=True,
    data_rate="10MHz",
    partial_update=True,
    # A full 6-color refresh on a panel this large takes tens of seconds; disallow
    # faster updates to avoid FSM update loops.
    minimum_update_interval="30s",
    # Default GPIO pins for the on-board Inkplate 13 Spectra wiring.
    dc_pin=14,
    cs_pin=42,
    cs1_pin=39,
    bs0_pin=6,
    bs1_pin=5,
    rst_pin=4,
    pwr_en_pin=21,
    busy_pin={
        "number": 7,
        "inverted": True,  # hardware: LOW=busy, HIGH=idle
        "mode": {
            "input": True,
            "pullup": True,
        },
    },
)
