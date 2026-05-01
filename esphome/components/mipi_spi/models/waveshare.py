from esphome.components.mipi import (
    ETMOD,
    FRMCTR2,
    GMCTRN1,
    GMCTRP1,
    IFCTR,
    MODE_RGB,
    PWCTR1,
    PWCTR3,
    PWCTR4,
    PWCTR5,
    PWSET,
    DriverChip,
)
import esphome.config_validation as cv

from .amoled import CO5300
from .ili import ILI9488_A, ST7789V
from .jc import AXS15231

DriverChip(
    "WAVESHARE-4-TFT",
    width=320,
    height=480,
    invert_colors=True,
    spi_16=True,
    initsequence=(
        (
            0xF9,
            0x00,
            0x08,
        ),
        (
            0xC0,
            0x19,
            0x1A,
        ),
        (
            0xC1,
            0x45,
            0x00,
        ),
        (
            0xC2,
            0x33,
        ),
        (
            0xC5,
            0x00,
            0x28,
        ),
        (
            0xB1,
            0xA0,
            0x11,
        ),
        (
            0xB4,
            0x02,
        ),
        (
            0xB6,
            0x00,
            0x42,
            0x3B,
        ),
        (
            0xB7,
            0x07,
        ),
        (
            0xE0,
            0x1F,
            0x25,
            0x22,
            0x0B,
            0x06,
            0x0A,
            0x4E,
            0xC6,
            0x39,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
            0x00,
        ),
        (
            0xE1,
            0x1F,
            0x3F,
            0x3F,
            0x0F,
            0x1F,
            0x0F,
            0x46,
            0x49,
            0x31,
            0x05,
            0x09,
            0x03,
            0x1C,
            0x1A,
            0x00,
        ),
        (
            0xF1,
            0x36,
            0x04,
            0x00,
            0x3C,
            0x0F,
            0x0F,
            0xA4,
            0x02,
        ),
        (
            0xF2,
            0x18,
            0xA3,
            0x12,
            0x02,
            0x32,
            0x12,
            0xFF,
            0x32,
            0x00,
        ),
        (
            0xF4,
            0x40,
            0x00,
            0x08,
            0x91,
            0x04,
        ),
        (
            0xF8,
            0x21,
            0x04,
        ),
    ),
)
ST7789P = DriverChip(
    "ST7789P",
    # Max supported dimensions
    width=240,
    height=320,
    # SPI: RGB layout
    color_order=MODE_RGB,
    invert_colors=True,
    draw_rounding=1,
)

ILI9488_A.extend(
    "PICO-RESTOUCH-LCD-3.5",
    swap_xy=cv.UNDEFINED,
    spi_16=True,
    pixel_mode="16bit",
    mirror_x=True,
    dc_pin=33,
    cs_pin=34,
    reset_pin=40,
    data_rate="20MHz",
    invert_colors=True,
)

CO5300.extend(
    "WAVESHARE-ESP32-S3-TOUCH-AMOLED-1.75",
    width=466,
    height=466,
    pixel_mode="16bit",
    offset_height=0,
    offset_width=6,
    cs_pin=12,
    reset_pin=39,
)

AXS15231.extend(
    "WAVESHARE-ESP32-S3-TOUCH-LCD-3.49",
    width=172,
    height=640,
    data_rate="80MHz",
    cs_pin=9,
    reset_pin=21,
)

# Waveshare 1.83-v2
#
# Do not use on 1.83-v1: Vendor warning on different chip!
ST7789P.extend(
    "WAVESHARE-1.83-V2",
    # Panel size smaller than ST7789 max allowed
    width=240,
    height=284,
    # Vendor specific init derived from vendor sample code
    # "LCD_1.83_Code_Rev2/ESP32/LCD_1in83/LCD_Driver.cpp"
    # Compatible MIT license, see esphome/LICENSE file.
    initsequence=(
        (FRMCTR2, 0x0C, 0x0C, 0x00, 0x33, 0x33),
        (ETMOD, 0x35),
        (0xBB, 0x19),
        (PWCTR1, 0x2C),
        (PWCTR3, 0x01),
        (PWCTR4, 0x12),
        (PWCTR5, 0x20),
        (IFCTR, 0x0F),
        (PWSET, 0xA4, 0xA1),
        (
            GMCTRP1,
            0xD0,
            0x04,
            0x0D,
            0x11,
            0x13,
            0x2B,
            0x3F,
            0x54,
            0x4C,
            0x18,
            0x0D,
            0x0B,
            0x1F,
            0x23,
        ),
        (
            GMCTRN1,
            0xD0,
            0x04,
            0x0C,
            0x11,
            0x13,
            0x2C,
            0x3F,
            0x44,
            0x51,
            0x2F,
            0x1F,
            0x1F,
            0x20,
            0x23,
        ),
    ),
)

ST7789V.extend(
    "WAVESHARE-ESP32-C6-LCD-1.47",
    width=172,
    height=320,
    offset_width=34,
    invert_colors=True,
    data_rate="40MHz",
    reset_pin=21,
    cs_pin=14,
    dc_pin={"number": 15, "ignore_strapping_warning": True},
)
