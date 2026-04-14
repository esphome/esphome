"""
Models for the IT8951 family.

The IT8951 does NOT use a DC pin; it uses 16-bit SPI command preambles instead.
"""

from . import EpaperModel

# Generic IT8951 model: requires explicit dimensions (no defaults provided)
EpaperModel(
    "it8951",
    class_name="EPaperIT8951",
    initsequence=(),
    dc_pin=False,
    reversed=False,
    sleep_when_done=True,
    vcom=2300,
    force_1bpp=False,
    data_rate=12_000_000,
)

# M5EPD-specific model with pre-filled dimensions
EpaperModel(
    "M5EPD",
    class_name="EPaperIT8951",
    width=960,
    height=540,
    initsequence=(),
    busy_pin=27,
    reset_pin=23,
    cs_pin=15,
    dc_pin=False,
    reversed=False,
    sleep_when_done=True,
    vcom=2300,
    force_1bpp=False,
    data_rate=20_000_000,
)

EpaperModel(
    "seeed-reterminal-e1003",
    class_name="EPaperIT8951",
    width=1872,
    height=1404,
    initsequence=(),
    busy_pin=13,
    reset_pin=12,
    cs_pin=10,
    enable_pin=[11, 21],
    dc_pin=False,
    reversed=False,
    sleep_when_done=False,
    vcom=1400,
    force_1bpp=False,
    data_rate=4_000_000,
)

EpaperModel(
    "seeed-ee03",
    class_name="EPaperIT8951",
    width=1872,
    height=1404,
    initsequence=(),
    busy_pin=4,
    reset_pin=38,
    cs_pin=44,
    enable_pin=[43],
    dc_pin=False,
    reversed=False,
    sleep_when_done=False,
    vcom=1400,
    force_1bpp=False,
    data_rate=4_000_000,
)
