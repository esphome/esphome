from .ili import ST7789V

ST7789V.extend(
    "ADAFRUIT-FUNHOUSE",
    height=240,
    width=240,
    offset_height=0,
    offset_width=0,
    cs_pin=40,
    dc_pin=39,
    reset_pin=41,
    invert_colors=True,
    mirror_x=True,
    mirror_y=True,
    data_rate="80MHz",
)

ST7789V.extend(
    "ADAFRUIT-S2-TFT-FEATHER",
    height=240,
    width=135,
    offset_height=52,
    offset_width=40,
    cs_pin=7,
    dc_pin=39,
    reset_pin=40,
    invert_colors=True,
)

models = {}
