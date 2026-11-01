from esphome.const import CONF_DATA_RATE

from . import EpaperModel


class SSD1683(EpaperModel):
    source_files = (
        "epaper_spi_mono.cpp",
        "epaper_spi_ssd1683.cpp",
    )

    def __init__(self, name, class_name="EPaperSSD1683", data_rate="20MHz", **defaults):
        defaults[CONF_DATA_RATE] = data_rate
        defaults.setdefault("partial_update", True)
        super().__init__(name, class_name=class_name, **defaults)

    # fmt: off
    def get_init_sequence(self, config: dict):
        _width, height = self.get_dimensions(config)
        return (
            (0x01, (height - 1) % 256, (height - 1) // 256, 0x00),    # Set column gate limit
            (0x18, 0x80),    # Select internal Temp sensor
            (0x11, 0x03),      # Set transform
        )


ssd1683 = SSD1683("ssd1683")

goodisplay_gdey042t81 = ssd1683.extend(
    "goodisplay-gdey042t81-4.2",
    width=400,
    height=300,
)

# Good Display GDEY029T94, 2.9" mono 128x296 (SSD1680), also sold by Seeed as 029BN-T94-D2.
# Replaces the waveshare_epaper model gdey029t94.
goodisplay_gdey029t94 = ssd1683.extend(
    "goodisplay-gdey029t94-2.9",
    width=128,
    height=296,
)
