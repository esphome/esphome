"""WeAct Black/White/Red e-paper displays using SSD1683 controller.

Supported models:
- weact-2.13in-3c: 122x250 pixels (2.13" display)
- weact-2.9in-3c: 128x296 pixels (2.9" display)
- weact-4.2in-3c: 400x300 pixels (4.2" display)

These displays use SSD1680 or SSD1683 controller and require a specific initialization
sequence. The DRV_OUT_CTL command is calculated from the display height.
"""

from . import EpaperModel


class WeActBWR(EpaperModel):
    """Base EpaperModel class for WeAct Black/White/Red displays using SSD1683 controller."""

    def __init__(self, name, **defaults):
        super().__init__(name, "EPaperWeAct3C", **defaults)

    def get_init_sequence(self, config):
        """Generate initialization sequence for WeAct BWR displays.

        The initialization sequence is based on SSD1680 and SSD1683 controller datasheet
        and the WeAct display specifications.
        """
        _, height = self.get_dimensions(config)
        # DRV_OUT_CTL: MSB of (height-1), LSB of (height-1), gate setting (0x00)
        height_minus_1 = height - 1
        msb = height_minus_1 >> 8
        lsb = height_minus_1 & 0xFF
        return (
            # Step 1: Software Reset (0x12) - REQUIRED per SSD1680, but works without it as well, so it's commented out for now
            # (0x12,),
            # Step 2: Wait 10ms after SWRESET (?) not sure how to implement wht waiting for 10ms after SWRESET, so it's commented out for now
            # Step 3: DRV_OUT_CTL - driver output control (height-dependent)
            # Format: (command, LSB, MSB, gate setting)
            (0x01, lsb, msb, 0x00),
            # Step 4: DATA_ENTRY - data entry mode (0x03 = decrement Y, increment X)
            (0x11, 0x03),
            # Step 5: BORDER_FULL - border waveform control
            (0x3C, 0x05),
            # Step 6: TEMP_SENS - internal temperature sensor
            (0x18, 0x80),
            # Step 7: DISPLAY_UPDATE - display update control
            (0x21, 0x00, 0x80),
        )


# Model: WeAct 2.9" 3C - 128x296 pixels, SSD1680 controller
weact_2p9in3c = WeActBWR(
    "weact-2.9in-3c",
    width=128,
    height=296,
    data_rate="10MHz",
    minimum_update_interval="1s",
)

# Model: WeAct 2.13" 3C - 122x250 pixels, SSD1680 controller
weact_2p13in3c = WeActBWR(
    "weact-2.13in-3c",
    width=122,
    height=250,
    data_rate="10MHz",
    minimum_update_interval="1s",
)

# Model: WeAct 4.2" 3C - 400x300 pixels, SSD1683 controller
weact_4p2in3c = WeActBWR(
    "weact-4.2in-3c",
    width=400,
    height=300,
    data_rate="10MHz",
    minimum_update_interval="10s",
)
