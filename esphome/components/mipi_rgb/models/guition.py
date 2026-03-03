from .st7701s import st7701s

st7701s.extend(
    "GUITION-4848S040",
    width=480,
    height=480,
    data_rate="2MHz",
    cs_pin=39,
    de_pin=18,
    hsync_pin=16,
    vsync_pin=17,
    pclk_pin=21,
    pclk_frequency="12MHz",
    pclk_inverted=False,
    pixel_mode="18bit",
    mirror_x=True,
    mirror_y=True,
    data_pins={
        "red": [11, 12, 13, 14, 0],
        "green": [8, 20, 3, 46, 9, 10],
        "blue": [4, 5, 6, 7, 15],
    },
    # Additional configuration for Guition 4848S040, 16 bit bus config
    add_init_sequence=((0xCD, 0x00),),
)
