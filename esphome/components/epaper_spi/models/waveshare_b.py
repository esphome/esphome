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
            (0x21, 0x80, 0x80),  # Display update control
        )


WaveshareB(
    "waveshare-2.13in-bv4",
    width=122,
    height=250,
    data_rate="10MHz",
    minimum_update_interval="1s",
)
