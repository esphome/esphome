import esphome.codegen as cg

from . import EpaperModel


class WaveshareB(EpaperModel):
    def __init__(self, name, **defaults):
        super().__init__(name, "EpaperWaveshareB", **defaults)

    def get_init_sequence(self, config):
        _, height = self.get_dimensions(config)
        h = height - 1
        return (
            (0x01, h & 0xFF, h >> 8, 0x00),  # Driver output control
            (0x11, 0x03),  # Data entry mode
            (0x3C, 0x05),  # Border waveform
            (0x18, 0x80),  # Internal temperature sensor
            (
                0x21,
                self.get_default("ram_option", 0x80),
                0x80,
            ),  # Display update control
        )

    async def to_code(self, var, config):
        cg.add(var.set_invert_red(self.get_default("invert_red", False)))
        return config


WaveshareB(
    "waveshare-2.13in-bv4",
    width=122,
    height=250,
    data_rate="10MHz",
    minimum_update_interval="1s",
    invert_red=True,
)

# Waveshare 2.66" B, SSD1680Z8. The red plane is not inverted on this panel, and byte A of
# the display update control differs from the 2.13" model.
WaveshareB(
    "waveshare-2.66in-b",
    width=152,
    height=296,
    data_rate="10MHz",
    minimum_update_interval="30s",  # a full tri-color refresh takes ~15s at 23C
    ram_option=0x00,
)
