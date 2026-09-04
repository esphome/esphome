# The panel is split into two halves, each with its own chip-select: CS (primary, left half
# of every row) and CS1 (secondary, right half). BS0/BS1 select the controllers' interface mode.
# RST and PWR_EN are driven directly by this driver (not through the generic reset_pin_ /
# enable_pins_ mechanisms) because the power-on sequence toggles them dynamically on every
# refresh cycle, not just once at setup.

from esphome import pins
import esphome.codegen as cg
from esphome.const import CONF_CS_PIN
from esphome.cpp_generator import MockObj

from . import EpaperModel

CONF_CS1_PIN = "cs1_pin"
CONF_BS0_PIN = "bs0_pin"
CONF_BS1_PIN = "bs1_pin"
CONF_PWR_EN_PIN = "pwr_en_pin"
# Not a CONF_RST_PIN constant: that name is already independently defined in 3+ other
# components, and ci-custom's duplicate-constant lint requires new definitions of an
# already-3x-duplicated constant to move into esphome/const.py in a separate PR. Using
# the literal string sidesteps that without renaming the actual "rst_pin" config key.
RST_PIN_KEY = "rst_pin"


class Inkplate13SpectraModel(EpaperModel):
    # The driver drives CS and CS1 directly for the dual-chip protocol.
    manages_cs = True

    def __init__(self, name, class_name="EPaperInkplate13Spectra", **defaults):
        super().__init__(name, class_name, **defaults)

    def get_constructor_args(self, config) -> tuple:
        # toggle_dc: this panel uses 3-wire SPI (BS0/BS1 select the mode), so
        # write_command_to_chip_() must not toggle the DC pin -- cmd/data is inferred by
        # byte position instead.
        return (False,)

    def get_config_options(self) -> dict:
        return {
            self.option(CONF_CS1_PIN, fallback=None): pins.gpio_output_pin_schema,
            self.option(CONF_BS0_PIN, fallback=None): pins.gpio_output_pin_schema,
            self.option(CONF_BS1_PIN, fallback=None): pins.gpio_output_pin_schema,
            self.option(RST_PIN_KEY, fallback=None): pins.gpio_output_pin_schema,
            self.option(CONF_PWR_EN_PIN, fallback=None): pins.gpio_output_pin_schema,
        }

    async def to_code(self, var: MockObj, config: dict) -> dict:
        cs = await cg.gpio_pin_expression(config[CONF_CS_PIN])
        cs1 = await cg.gpio_pin_expression(config[CONF_CS1_PIN])
        cg.add(var.set_cs_pins(cs, cs1))
        bs0 = await cg.gpio_pin_expression(config[CONF_BS0_PIN])
        bs1 = await cg.gpio_pin_expression(config[CONF_BS1_PIN])
        cg.add(var.set_bs_pins(bs0, bs1))
        rst = await cg.gpio_pin_expression(config[RST_PIN_KEY])
        cg.add(var.set_rst_pin(rst))
        pwr_en = await cg.gpio_pin_expression(config[CONF_PWR_EN_PIN])
        cg.add(var.set_pwr_en_pin(pwr_en))
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
                RST_PIN_KEY,
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
