from esphome.components.mipi import DriverChip
import esphome.config_validation as cv

# fmt: off
DriverChip(
    "WAVESHARE-ESP32-P4-WIFI6-TOUCH-LCD-7B",
    height=600,
    width=1024,
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
        (0xE0, 0x00),
        (0xE1, 0x93),
        (0xE2, 0x65),
        (0xE3, 0xF8),
        (0xE6, 0x64),
        (0xE7, 0x66),
        (0x80, 0x8B),
        (0x81, 0x78),
        (0x82, 0x84),
        (0x83, 0x88),
        (0x84, 0xA8),
        (0x85, 0xE3),
        (0x86, 0x88),
        (0xB2, 0x50),
        (0xC0, 0x01, 0x09),
        (0xC1, 0x41),
        (0xC5, 0x00, 0x0A, 0x80),
        (0x36, 0x03),  # MADCTL par défaut (paysage normal, conforme EK79007)
        (0x3A, 0x77),
        (0xE8, 0x84, 0x11, 0x79),
        (0xEC, 0x7B),
        (0x11, 0x00),
        (0x29, 0x00),
    ],
)
# fmt: on
