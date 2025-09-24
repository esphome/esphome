from esphome.components.spi import TYPE_OCTAL

from .. import MODE_BGR
from .ili import ST7789V, ST7796

ST7789V.extend(
    "T-EMBED",
    width=170,
    height=320,
    offset_width=35,
    color_order=MODE_BGR,
    invert_colors=True,
    draw_rounding=1,
    cs_pin=10,
    dc_pin=13,
    reset_pin=9,
    data_rate="80MHz",
)

ST7789V.extend(
    "T-DISPLAY",
    height=240,
    width=135,
    offset_width=52,
    offset_height=40,
    draw_rounding=1,
    cs_pin=5,
    dc_pin=16,
    invert_colors=True,
)
ST7789V.extend(
    "T-DISPLAY-S3",
    height=320,
    width=170,
    offset_width=35,
    color_order=MODE_BGR,
    invert_colors=True,
    draw_rounding=1,
    dc_pin=7,
    cs_pin=6,
    reset_pin=5,
    enable_pin=[9, 15],
    data_rate="10MHz",
    bus_mode=TYPE_OCTAL,
)

ST7796.extend(
    "T-DISPLAY-S3-PRO",
    width=222,
    height=480,
    offset_width=49,
    draw_rounding=1,
    cs_pin=39,
    reset_pin=47,
    dc_pin=9,
    backlight_pin=48,
    invert_colors=True,
)

models = {}
