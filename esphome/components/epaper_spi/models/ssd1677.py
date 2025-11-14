from esphome.const import CONF_DATA_RATE

from . import EpaperModel


class SSD1677(EpaperModel):
    def __init__(self, name, class_name="EPaperSSD1677", **kwargs):
        if CONF_DATA_RATE not in kwargs:
            kwargs[CONF_DATA_RATE] = "20MHz"
        super().__init__(name, class_name, **kwargs)

    # fmt: off
    def get_init_sequence(self, config: dict):
        width, _height = self.get_dimensions(config)
        return (
            (0x18, 0x80),    # Select internal Temp sensor
            (0x0C, 0xAE, 0xC7, 0xC3, 0xC0, 0x80),  # inrush current level 2
            (0x01, (width - 1) % 256, (width - 1) // 256, 0x02),    # Set column gate limit
            (0x3C, 0x01),    # Set border waveform
            (0x11, 3),      # Set transform
        )


ssd1677 = SSD1677("ssd1677")

ssd1677.extend(
    "seeed-ee04-mono-4.26",
    width=800,
    height=480,
    mirror_x=True,
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
