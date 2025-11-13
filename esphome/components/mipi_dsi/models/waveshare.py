from esphome.components.mipi import DriverChip
import esphome.config_validation as cv

# fmt: off
DriverChip(
    "WAVESHARE-ESP32-P4-WIFI6-TOUCH-LCD-7B",
    width=1024,
    height=600,
    hsync_back_porch=160,
    hsync_pulse_width=10,
    hsync_front_porch=160,
    vsync_back_porch=23,
    vsync_pulse_width=1,
    vsync_front_porch=12,
    pclk_frequency="52MHz",
    lane_bit_rate="1Gbps",
    swap_xy=cv.UNDEFINED,
    color_order="RGB",
    reset_pin=33,
    initsequence=[
        (0xB2, 0x10),
        (0x80, 0x8B),
        (0x81, 0x78),
        (0x82, 0x84),
        (0x83, 0x88),
        (0x84, 0xA8),
        (0x85, 0xE3),
        (0x86, 0x88),
        (0xB2, 0x10),
        (0x36, 0x01), 
        (0x11, 0x00),
        (0x29, 0x00),
    ],
)
