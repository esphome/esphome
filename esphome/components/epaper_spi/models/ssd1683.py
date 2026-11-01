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

# Good Display GDEY0213B74, 2.13" mono 122x250, SSD1680; also sold by Seeed as 0213BN-B74.
# Replaces the waveshare_epaper model 2.13in-ttgo-b74.
goodisplay_gdey0213b74 = ssd1683.extend(
    "goodisplay-gdey0213b74-2.13",
    width=122,
    height=250,
)

# Name printed on the panel's label as sold by Seeed
goodisplay_gdey0213b74.extend("0213bn-b74")

# LilyGO TTGO T5 2.13" board with the GDEY0213B74 panel
goodisplay_gdey0213b74.extend("ttgo-t5-2.13in-b74")
