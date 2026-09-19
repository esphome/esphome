"""T133A01-based e-paper displays.

The T133A01 is a 6-color e-paper controller IC that drives large panels
(1200x1600 portrait). It uses a dual-CS SPI architecture where CS
controls one half of the pixel data and CS1 controls the other half,
as well as panel-level commands (power on, refresh, power off).

Supported models:
- Seeed-reTerminal-E1004: 1200x1600 pixels, 6-color (T133A01 panel)
"""

from esphome import pins
import esphome.codegen as cg
from esphome.const import CONF_CS_PIN
from esphome.cpp_generator import MockObj

from . import EpaperModel

CONF_CS1_PIN = "cs1_pin"


class T133A01Model(EpaperModel):
    """EpaperModel subclass for T133A01-based 6-color e-paper displays."""

    # The driver drives CS and CS1 directly for the dual-CS protocol.
    manages_cs = True

    def __init__(self, name, class_name="EPaperT133A01", **defaults):
        super().__init__(name, class_name, **defaults)

    def get_config_options(self) -> dict:
        # CS1 is the second chip-select required by the dual-CS architecture.
        # fallback=None makes it required unless the model provides a default.
        return {
            self.option(CONF_CS1_PIN, fallback=None): pins.gpio_output_pin_schema,
        }

    async def to_code(self, var: MockObj, config: dict) -> dict:
        cs = await cg.gpio_pin_expression(config[CONF_CS_PIN])
        cs1 = await cg.gpio_pin_expression(config[CONF_CS1_PIN])
        cg.add(var.set_cs_pins(cs, cs1))
        # Remove CS and CS1 from the config so that the base class doesn't try to handle them.
        return {k: v for k, v in config.items() if k not in (CONF_CS_PIN, CONF_CS1_PIN)}


t133a01_base = T133A01Model(
    "t133a01",
    minimum_update_interval="30s",
    data_rate="10MHz",
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
