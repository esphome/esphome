from esphome.components.spi import TYPE_QUAD

from .. import MODE_RGB
from . import DriverChip, delay
from .commands import MIPI, NORON, PAGESEL, PIXFMT, SLPOUT, SWIRE1, SWIRE2, TEON, WRAM

DriverChip(
    "T-DISPLAY-S3-AMOLED",
    width=240,
    height=536,
    cs_pin=6,
    reset_pin=17,
    enable_pin=38,
    bus_mode=TYPE_QUAD,
    brightness=0xD0,
    color_order=MODE_RGB,
    initsequence=(SLPOUT,),  # Requires early SLPOUT
)

DriverChip(
    name="T-DISPLAY-S3-AMOLED-PLUS",
    width=240,
    height=536,
    cs_pin=6,
    reset_pin=17,
    dc_pin=7,
    enable_pin=38,
    data_rate="40MHz",
    brightness=0xD0,
    color_order=MODE_RGB,
    initsequence=(
        (PAGESEL, 4),
        (0x6A, 0x00),
        (PAGESEL, 0x05),
        (PAGESEL, 0x07),
        (0x07, 0x4F),
        (PAGESEL, 0x01),
        (0x2A, 0x02),
        (0x2B, 0x73),
        (PAGESEL, 0x0A),
        (0x29, 0x10),
        (PAGESEL, 0x00),
        (0x53, 0x20),
        (TEON, 0x00),
        (PIXFMT, 0x75),
        (0xC4, 0x80),
    ),
)

RM690B0 = DriverChip(
    "RM690B0",
    brightness=0xD0,
    color_order=MODE_RGB,
    width=480,
    height=600,
    initsequence=(
        (PAGESEL, 0x20),
        (MIPI, 0x0A),
        (WRAM, 0x80),
        (SWIRE1, 0x51),
        (SWIRE2, 0x2E),
        (PAGESEL, 0x00),
        (0xC2, 0x00),
        delay(10),
        (TEON, 0x00),
        (NORON,),
    ),
)

T4_S3_AMOLED = RM690B0.extend("T4-S3", width=450, offset_width=16, bus_mode=TYPE_QUAD)

models = {}
