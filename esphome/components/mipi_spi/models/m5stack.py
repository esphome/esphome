from esphome.components.mipi import (
    DFUNCTR,
    GMCTRN1,
    GMCTRP1,
    IDMOFF,
    IFMODE,
    PWCTR1,
    PWCTR2,
    SETEXTC,
    VMCTR1,
    DriverChip,
)

from .ili import ILI9341, ST7789V

# fmt: off
DriverChip(
    "M5CORE",
    width=320,
    height=240,
    cs_pin=14,
    dc_pin=27,
    reset_pin=33,
    initsequence=(
        (SETEXTC, 0xFF, 0x93, 0x42),
        (PWCTR1, 0x12, 0x12),
        (PWCTR2, 0x03),
        (VMCTR1, 0xF2),
        (IFMODE, 0xE0),
        (0xF6, 0x01, 0x00, 0x00),
        (GMCTRP1, 0x00, 0x0C, 0x11, 0x04, 0x11, 0x08, 0x37, 0x89, 0x4C, 0x06, 0x0C, 0x0A, 0x2E, 0x34, 0x0F,),
        (GMCTRN1, 0x00, 0x0B, 0x11, 0x05, 0x13, 0x09, 0x33, 0x67, 0x48, 0x07, 0x0E, 0x0B, 0x2E, 0x33, 0x0F,),
        (DFUNCTR, 0x08, 0x82, 0x1D, 0x04),
        (IDMOFF,),
    ),
)

# M5Stack Core2 uses ILI9341 chip - mirror_x disabled for correct orientation
ILI9341.extend(
    "M5CORE2",
    # Reset native dimensions due to axis swap.
    native_width=320,
    native_height=240,
    width=320,
    height=240,
    mirror_x=False,
    cs_pin=5,
    dc_pin=15,
    invert_colors=True,
    pixel_mode="18bit",
    data_rate="40MHz",
    requires={"psram"},
)

GC9107 = ST7789V.extend(
    "GC9107",
    width=128,
    height=128,
    offset_width=2,
    offset_height=1,
    pad_width=2,
    pad_height=1,
)

GC9107.extend(
    "M5STACK-ATOMS3R-GC9107",
    data_rate="40MHz",
    invert_colors=True,
    reset_pin=48,
    dc_pin=42,
    cs_pin=14,
    requires={"psram"},
)
