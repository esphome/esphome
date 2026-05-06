from . import EpaperModel


class Waveshare7p3inG(EpaperModel):
    """Waveshare 7.3 inch HAT (G): black / white / red / yellow.

    800x480 framebuffer - epd7in3g wiring, 2 bpp, 4 pixels per byte."""

    def __init__(self, name, class_name="EPaperEpd7In3G", **defaults):
        super().__init__(name, class_name, **defaults)

    def get_init_sequence(self, config: dict):
        width, height = self.get_dimensions(config)
        return (
            (30, 0xFF),  # post-reset settle
            (0xAA, 0x49, 0x55, 0x20, 0x08, 0x09, 0x18),
            (0x01, 0x3F),
            (0x00, 0x4F, 0x69),
            (0x05, 0x40, 0x1F, 0x1F, 0x2C),
            (0x08, 0x6F, 0x1F, 0x1F, 0x22),
            (0x06, 0x6F, 0x1F, 0x14, 0x14),
            (0x03, 0x00, 0x54, 0x00, 0x44),
            (0x60, 0x02, 0x00),
            (0x30, 0x08),
            (0x50, 0x3F),
            (
                0x61,
                width // 256,
                width % 256,
                height // 256,
                height % 256,
            ),
            (0xE3, 0x2F),
            (0x84, 0x01),
        )


waveshare_7p3in_g = Waveshare7p3inG(
    "Waveshare-7.3in-G",
    width=800,
    height=480,
    minimum_update_interval="30s",
    data_rate="20MHz",
)
