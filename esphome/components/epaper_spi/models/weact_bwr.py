"""WeAct Black/White/Red e-paper displays using SSD1683 controller.

Supported models:
- weact-2.13in-3c: 250x122 pixels (2.13" display)
- weact-2.9in-3c: 128x296 pixels (2.9" display)
- weact-4.2in-3c: 400x300 pixels (4.2" display)

These displays use the SSD1683 controller and require a specific initialization
sequence. The DRV_OUT_CTL command differs based on the display height.
"""

from . import EpaperModel


class WeActBWR(EpaperModel):
    """Base EpaperModel class for WeAct Black/White/Red displays using SSD1683 controller."""

    def __init__(self, name, drv_out_ctl, **defaults):
        self.drv_out_ctl_ = drv_out_ctl
        super().__init__(name, "EPaperWeAct3C", **defaults)

    def get_init_sequence(self, config):
        """Generate initialization sequence for WeAct BWR displays.

        The initialization sequence is based on the SSD1683 controller datasheet
        and the WeAct display specifications.
        """
        return (
            # DRV_OUT_CTL - driver output control (height-dependent)
            # Format: (command, MSB height, LSB height, gate setting)
            (0x01, self.drv_out_ctl_[0], self.drv_out_ctl_[1], self.drv_out_ctl_[2]),
            # DATA_ENTRY - data entry mode (0x03 = decrement Y, increment X)
            (0x11, 0x03),
            # BORDER_FULL - border waveform control
            (0x3C, 0x05),
            # TEMP_SENS - internal temperature sensor
            (0x18, 0x80),
            # DISPLAY_UPDATE - display update control
            (0x21, 0x00, 0x80),
        )


# Model: WeAct 2.9" 3C - 128x296 pixels, SSD1683 controller
weact_2p9in3c = WeActBWR(
    "weact-2.9in-3c",
    drv_out_ctl=(0x27, 0x01, 0x00),  # Height = 0x127 = 296
    width=128,
    height=296,
    data_rate="10MHz",
    minimum_update_interval="1s",
)

# Model: WeAct 2.13" 3C - 250x122 pixels, SSD1683 controller
weact_2p13in3c = WeActBWR(
    "weact-2.13in-3c",
    drv_out_ctl=(0x7A, 0x00, 0x00),  # Height = 0x7A = 122
    width=250,
    height=122,
    data_rate="10MHz",
    minimum_update_interval="1s",
)

# Model: WeAct 4.2" 3C - 400x300 pixels, SSD1683 controller
weact_4p2in3c = WeActBWR(
    "weact-4.2in-3c",
    drv_out_ctl=(0xCF, 0x01, 0x00),  # Height = 0x12C = 300
    width=400,
    height=300,
    data_rate="10MHz",
    minimum_update_interval="10s",
)
