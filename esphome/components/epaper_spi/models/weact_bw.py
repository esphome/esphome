"""WeAct black/white e-paper displays using SSD1680/SSD1683-compatible command flow.

Currently only the tested 4.2 inch BW panel is enabled here for safe rollout.
"""

from . import EpaperModel


class WeActBW(EpaperModel):
    def __init__(self, name, **defaults):
        super().__init__(name, "EPaperWeActBW", **defaults)

    def get_init_sequence(self, config):
        _, height = self.get_dimensions(config)
        height_minus_1 = height - 1
        msb = height_minus_1 >> 8
        lsb = height_minus_1 & 0xFF
        return (
            (0x01, lsb, msb, 0x00),
            (0x11, 0x03),
            (0x3C, 0x05),
            (0x18, 0x80),
            (0x21, 0x00, 0x80),
        )


# WeAct 4.2 inch BW panel: 400x300, SSD1683.
WeActBW(
    "weact-4.2in-bw",
    width=400,
    height=300,
    data_rate="10MHz",
    minimum_update_interval="10s",
)