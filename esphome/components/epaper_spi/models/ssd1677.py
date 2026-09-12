from esphome.const import CONF_DATA_RATE

from . import EpaperModel


class SSD1677(EpaperModel):
    def __init__(self, name, class_name="EPaperMono", data_rate="20MHz", **defaults):
        defaults[CONF_DATA_RATE] = data_rate
        super().__init__(name, class_name, **defaults)

    # fmt: off
    def get_init_sequence(self, config: dict):
        _width, height = self.get_dimensions(config)
        return (
            (0x18, 0x80),    # Select internal Temp sensor
            (0x0C, 0xAE, 0xC7, 0xC3, 0xC0, 0x80),  # inrush current level 2
            (0x01, (height - 1) % 256, (height - 1) // 256, 0x02),    # Set gate limit (number of rows-1)
            (0x3C, 0x01),    # Set border waveform
            (0x11, 3),      # Set transform
        )


class SSD1677Gray4(SSD1677):
    """Four-level variant.

    The panel film is monochrome; the levels are synthesised from both RAM
    planes by the panel's own OTP waveform. Only the border differs from the
    black and white bring-up: four-level follows LUT0, mono follows LUT1.
    """

    def __init__(self, name, **defaults):
        super().__init__(name, class_name="EPaperStickyGray4", **defaults)

    def get_init_sequence(self, config: dict):
        return tuple(
            (0x3C, 0x00) if cmd[0] == 0x3C else cmd
            for cmd in super().get_init_sequence(config)
        )


ssd1677 = SSD1677("ssd1677")

wave_4_26 = ssd1677.extend(
    "waveshare-4.26in",
    width=800,
    height=480,
    mirror_x=True,
)

wave_4_26.extend(
    "seeed-ee04-mono-4.26",
    cs_pin=44,
    dc_pin=10,
    reset_pin=38,
    busy_pin={
        "number": 4,
        "inverted": False,
        "mode": {
            "input": True,
            "pulldown": True,
        },
    },
)


ssd1677.extend(
    "waveshare-3.97in",
    width=800,
    height=480,
    mirror_x=True,
)

SEEED_STICKY = {
    "width": 800,
    "height": 480,
    "mirror_x": True,
    "enable_pin": 47,
    "cs_pin": 15,
    "dc_pin": 16,
    "reset_pin": 17,
    "busy_pin": 18,
    "data_rate": "10MHz",
}

ssd1677.extend("seeed-reterminal-sticky", **SEEED_STICKY)

# Same panel, driven in its four-level mode. The film is mono; the levels
# come from the vendor OTP waveform reading both RAM planes at once.
SSD1677Gray4("ssd1677-gray4").extend("seeed-reterminal-sticky-gray4", **SEEED_STICKY)
