from esphome.components.mipi import DriverChip
from esphome.config_validation import UNDEFINED

# fmt: off
sunton = DriverChip(
    "ESP32-8048S070",
    swap_xy=UNDEFINED,
    initsequence=(),
    width=800,
    height=480,
    pclk_frequency="12.5MHz",
    de_pin=41,
    hsync_pin=39,
    vsync_pin=40,
    pclk_pin=42,
    hsync_pulse_width=30,
    hsync_back_porch=16,
    hsync_front_porch=210,
    vsync_pulse_width=13,
    vsync_back_porch=10,
    vsync_front_porch=22,
    data_pins={
        "red": [14, 21, 47, 48, 45],
        "green": [9, 46, 3, 8, 16, 1],
        "blue": [15, 7, 6, 5, 4],
    },
)

sunton.extend(
    "ESP32-8048S050",
    swap_xy=UNDEFINED,
    initsequence=(),
    width=800,
    height=480,
    pclk_frequency="16MHz",
    de_pin=40,
    hsync_pin=39,
    vsync_pin=41,
    pclk_pin=42,
    hsync_back_porch=8,
    hsync_front_porch=8,
    hsync_pulse_width=4,
    vsync_back_porch=8,
    vsync_front_porch=8,
    vsync_pulse_width=4,
    data_pins={
        "red": [45, 48, 47, 21, 14],
        "green": [5, 6, 7, 15, 16, 4],
        "blue": [8, 3, 46, 9, 1],
    },
)
