from .ili import ST7789V

ST7789V.extend(
    "LANBON-L8",
    width=240,
    height=320,
    mirror_x=True,
    mirror_y=True,
    data_rate="80MHz",
    cs_pin=22,
    dc_pin=21,
    reset_pin=18,
)

models = {}
