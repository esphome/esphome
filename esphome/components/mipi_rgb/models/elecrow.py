from . import RgbDriverChip

# fmt: off
RgbDriverChip(
    "CROWPANEL-ADVANCE-7",
    requires={"psram"},
    initsequence=(),
    pclk_frequency="20MHz",
    hsync_pulse_width=4,
    hsync_front_porch=8,
    hsync_back_porch=8,
    vsync_pulse_width=4,
    vsync_front_porch=8,
    vsync_back_porch=8,
    pclk_inverted=True,
    color_order="RGB",
    width=800,
    height=480,
    de_pin=42,
    hsync_pin=40,
    vsync_pin=41,
    pclk_pin=39,
    data_pins={
        "red": [7, 17, 18, 3, 46],
        "green": [9, 10, 11, 12, 13, 14],
        "blue": [21, 47, 48, 45, 38],
    },
)
