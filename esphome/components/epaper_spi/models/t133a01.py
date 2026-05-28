"""T133A01-based e-paper displays.

The T133A01 is a 6-color e-paper controller IC that drives large panels
(1200x1600 portrait). It uses a dual-CS SPI architecture where CS
controls one half of the pixel data and CS1 controls the other half,
as well as panel-level commands (power on, refresh, power off).

Supported models:
- Seeed-reTerminal-E1004: 1200x1600 pixels, 6-color (T133A01 panel)
"""

from esphome.const import CONF_CS_PIN, CONF_DC_PIN, CONF_RESET_PIN, CONF_BUSY_PIN

from . import EpaperModel


class T133A01Model(EpaperModel):
    """EpaperModel subclass for T133A01-based 6-color e-paper displays."""

    def __init__(self, name, class_name="EPaperT133A01", **defaults):
        super().__init__(name, class_name, **defaults)


t133a01_base = T133A01Model(
    "t133a01-base",
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
    # CS (GPIO10) is managed by the SPI delegate. CS1 (GPIO2) is managed
    # via manual GPIO control for the dual-CS architecture.
    # For CS1 operations, GPIO10 is overridden HIGH after enable() to
    # prevent both chip selects from being active simultaneously.
    cs_pin=10,
    cs1_pin=2,
    dc_pin=11,
    reset_pin=38,
    busy_pin={
        "number": 13,
        "inverted": True,
        "mode": {"input": True},
    },
    enable_pin={
        "number": 12,
        "inverted": False,
        "mode": {
            "output": True,
        },
    },
)
