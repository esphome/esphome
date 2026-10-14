from esphome.const import CONF_DATA_RATE

from . import EpaperModel


class SSD1680(EpaperModel):
    def __init__(self, name: str, **defaults) -> None:
        defaults[CONF_DATA_RATE] = defaults.get(CONF_DATA_RATE, "10MHz")
        defaults.setdefault("partial_update", True)
        super().__init__(name, "EPaperSSD1680", **defaults)

    def get_init_sequence(self, config: dict):
        width, height = self.get_dimensions(config)
        return (
            (0x01, (height - 1) % 256, (height - 1) // 256, 0x00),
            (0x11, 0x03),
            (0x44, 0x00, (width + 7) // 8 - 1),
            (0x45, 0x00, 0x00, (height - 1) % 256, (height - 1) // 256),
            (0x4E, 0x00),
            (0x4F, 0x00, 0x00),
            (0x3C, 0x05),
            (0x21, 0x00, 0x80),
            (0x18, 0x80),
        )


weact_2p9in = SSD1680(
    "weact-2.9in",
    width=128,
    height=296,
)
