from esphome.const import CONF_MIRROR_X, CONF_MIRROR_Y, CONF_SWAP_XY, CONF_TRANSFORM

from . import EpaperModel


class SSD1677(EpaperModel):
    def __init__(self, name, class_name="EPaperSSD1677", **kwargs):
        super().__init__(name, class_name, **kwargs)

    # fmt: off
    def get_init_sequence(self, config: dict):
        width, height = self.get_dimensions(config)
        transform = 3
        initial_x = 0
        initial_y = 0
        if transforms := config.get(CONF_TRANSFORM):
            if transforms.get(CONF_SWAP_XY):
                transform |= 0x04
            if transforms.get(CONF_MIRROR_X):
                transform &= ~1
                initial_x = width - 1
            if transforms.get(CONF_MIRROR_Y):
                transform &= ~2
                initial_y = height - 1
        return (
            (0x18, 0x80),    # Select internal Temp sensor
            (0x0C, 0xAE, 0xC7, 0xC3, 0xC0, 0x80),  # inrush current level 2
            (0x01, (width - 1) % 256, (width - 1) // 256, 0x02),    # Set column gate limit
            (0x3C, 0x01),    # Set border waveform
            (0x11, transform),  # Set transform
            (0x4E, initial_x % 256, initial_x // 256),   # Initial RAM X address
            (0x4F, initial_y % 256, initial_y // 256),   # Initial RAM Y address
        )

    def get_available_transforms(self):
        return {CONF_MIRROR_X, CONF_MIRROR_Y, CONF_SWAP_XY}


ssd1677 = SSD1677("ssd1677")
