"""WeAct Black/White/Red e-paper displays using SSD1683 controller.

Supported models:
- weact-2.9in-3c: 128x296 pixels (2.9" display)
- weact-4.2in-3c: 400x300 pixels (4.2" display)

These displays use the SSD1683 controller and require a specific initialization
sequence. The DRV_OUT_CTL command differs based on the display height.
"""

from . import EpaperModel


class WeActBWR(EpaperModel):
    """Base(EpaperModel class for WeAct Black/White/Red displays using SSD1683 controller."""

    def __init__(self, name, width, height, drv_out_ctl, **defaults):
        self.width_ = width
        self.height_ = height
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

    def get_dimensions(self, config):
        """Return the display dimensions."""
        return self.width_, self.height_

    def get_default(self, key, fallback=False):
        """Return default values with width/height fallbacks."""
        if key == "width":
            return self.width_
        if key == "height":
            return self.height_
        return super().get_default(key, fallback)


# Model: WeAct 2.9" 3C - 128x296 pixels, SSD1683 controller
weact_2p9in3c = WeActBWR(
    "weact-2.9in-3c",
    width=128,
    height=296,
    drv_out_ctl=(0x27, 0x01, 0x00),  # Height = 0x127 = 296
    data_rate="10MHz",
    minimum_update_interval="1s",
)

# Model: WeAct 4.2" 3C - 400x300 pixels, SSD1683 controller
weact_4p2in3c = WeActBWR(
    "weact-4.2in-3c",
    width=400,
    height=300,
    drv_out_ctl=(0x2B, 0x01, 0x00),  # Height = 0x12B = 300
    data_rate="10MHz",
    minimum_update_interval="10s",
)
