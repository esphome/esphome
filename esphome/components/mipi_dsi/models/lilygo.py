from esphome.components.mipi import delay

from . import DsiDriverChip

# fmt: off

# Source for parameters and initsequence:
#   https://github.com/Xinyuan-LilyGO/lilygo_device_driver (src/device/t_display_p4)
#   https://github.com/Llgok/cpp_bus_driver (src/chip/mipi/rm69a10.h)
# Product page: https://wiki.lilygo.cc/products/t-display-series/t-display-p4/
DsiDriverChip(
    "LILYGO-T-DISPLAY-P4-AMOLED",
    width=568,
    height=1232,
    hsync_back_porch=150,
    hsync_pulse_width=50,
    hsync_front_porch=50,
    vsync_back_porch=120,
    vsync_pulse_width=40,
    vsync_front_porch=80,
    pclk_frequency="60MHz",
    lane_bit_rate="1Gbps",
    color_order="RGB",
    brightness=0xFF,
    requires={"psram"},
    reset_pin={"xl9535": {"address": 0x20}, "number": 2},
    initsequence=[
        # The panel ignores commands sent too soon after the hardware reset
        delay(120),
        (0xFE, 0xFD),
        (0x80, 0xFC),
        (0xFE, 0x00),
        (0x2A, 0x00, 0x00, 0x02, 0x37),  # Columns 0 to 567
        (0x2B, 0x00, 0x00, 0x04, 0xCF),  # Rows 0 to 1231
        (0x31, 0x00, 0x03, 0x02, 0x34),
        (0x30, 0x00, 0x00, 0x04, 0xCF),
        (0x12, 0x00),
        (0x35, 0x00),  # Tearing effect output on
    ],
)
