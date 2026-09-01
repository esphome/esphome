from esphome.const import CONF_DATA_RATE

from . import EpaperModel


class SSD1683(EpaperModel):
    def __init__(self, name, class_name="EPaperSSD1683", data_rate="20MHz", **defaults):
        defaults[CONF_DATA_RATE] = data_rate
        super().__init__(name, class_name, **defaults)

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

# The SSD1681 shares the SSD1683 command set for everything this driver uses:
# the same 0x01/0x11/0x18 initialisation, the same 0x44/0x45 window and
# 0x4E/0x4F cursor registers with a byte-addressed X and a 16-bit Y, and the
# same 0x22/0x20 update. It only differs in the panel sizes it drives.
ssd1681 = SSD1683("ssd1681")

waveshare_1_54in_v2 = ssd1681.extend(
    "waveshare-1.54in-v2",
    width=200,
    height=200,
)
