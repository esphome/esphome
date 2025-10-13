from esphome.components.mipi import DriverChip
from esphome.config_validation import UNDEFINED

from .st7701s import st7701s

wave_4_3 = DriverChip(
    "ESP32-S3-TOUCH-LCD-4.3",
    swap_xy=UNDEFINED,
    initsequence=(),
    color_order="RGB",
    width=800,
    height=480,
    pclk_frequency="16MHz",
    reset_pin={"ch422g": None, "number": 3},
    enable_pin={"ch422g": None, "number": 2},
    de_pin=5,
    hsync_pin={"number": 46, "ignore_strapping_warning": True},
    vsync_pin={"number": 3, "ignore_strapping_warning": True},
    pclk_pin=7,
    pclk_inverted=True,
    hsync_front_porch=210,
    hsync_pulse_width=30,
    hsync_back_porch=30,
    vsync_front_porch=4,
    vsync_pulse_width=4,
    vsync_back_porch=4,
    data_pins={
        "red": [1, 2, 42, 41, 40],
        "green": [39, 0, 45, 48, 47, 21],
        "blue": [14, 38, 18, 17, 10],
    },
)

wave_4_3.extend(
    "WAVESHARE-5-1024X600",
    width=1024,
    height=600,
    hsync_back_porch=145,
    hsync_front_porch=170,
    hsync_pulse_width=30,
    vsync_back_porch=23,
    vsync_front_porch=12,
    vsync_pulse_width=2,
)

wave_4_3.extend(
    "ESP32-S3-TOUCH-LCD-7-800X480",
    enable_pin=[{"ch422g": None, "number": 2}, {"ch422g": None, "number": 6}],
    hsync_back_porch=8,
    hsync_front_porch=8,
    hsync_pulse_width=4,
    vsync_back_porch=16,
    vsync_front_porch=16,
    vsync_pulse_width=4,
)

st7701s.extend(
    "WAVESHARE-4-480x480",
    data_rate="2MHz",
    spi_mode="MODE3",
    color_order="BGR",
    pixel_mode="18bit",
    width=480,
    height=480,
    invert_colors=True,
    cs_pin=42,
    de_pin=40,
    hsync_pin=38,
    vsync_pin=39,
    pclk_pin=41,
    pclk_frequency="12MHz",
    pclk_inverted=False,
    data_pins={
        "red": [46, 3, 8, 18, 17],
        "green": [14, 13, 12, 11, 10, 9],
        "blue": [5, 45, 48, 47, 21],
    },
)
