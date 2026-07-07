from esphome.components.mipi import DriverChip
import esphome.config_validation as cv

# Standalone display
# Product page: https://www.seeedstudio.com/reTerminal-D1001-p-6729.html
DriverChip(
    "SEEED-RETERMINAL-D1001",
    height=1280,
    width=800,
    hsync_back_porch=20,
    hsync_pulse_width=20,
    hsync_front_porch=40,
    vsync_back_porch=12,
    vsync_pulse_width=4,
    vsync_front_porch=30,
    pclk_frequency="80MHz",
    lane_bit_rate="1.5Gbps",
    swap_xy=cv.UNDEFINED,
    color_order="RGB",
    enable_pin=[{"xl9535": None, "number": 0}, {"xl9535": None, "number": 7}],
    reset_pin={"xl9535": None, "number": 2},
    initsequence=(
        (0xE0, 0x00),
        (0xE1, 0x93),
        (0xE2, 0x65),
        (0xE3, 0xF8),
        (0x80, 0x01),
    ),
)
