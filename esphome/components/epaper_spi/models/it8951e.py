"""
Models for the IT8951E family.

We register two models:
- `it8951e`: generic driver entry. Requires explicit `dimensions` in the config.
- `M5EPD`: device-specific defaults for the M5EPD (960x540) which uses the same
  `EPaperIT8951E` C++ implementation but provides convenient defaults.

The IT8951E does NOT use a DC pin; it uses 16-bit SPI command preambles instead.
"""

from . import EpaperModel

# Generic IT8951E model: requires explicit dimensions (no defaults provided)
EpaperModel(
    "it8951e",
    class_name="EPaperIT8951E",
    initsequence=(),
    dc_pin=False,
)

# M5EPD-specific model with pre-filled dimensions
EpaperModel(
    "M5EPD",
    class_name="EPaperIT8951E",
    width=960,
    height=540,
    initsequence=(),
    busy_pin=27,
    reset_pin=23,
    cs_pin=15,
    dc_pin=False,
)
