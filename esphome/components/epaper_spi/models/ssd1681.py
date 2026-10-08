from typing import Any

import esphome.config_validation as cv
from esphome.const import CONF_DIMENSIONS
from esphome.types import ConfigType

from .ssd1683 import SSD1683

MAX_WIDTH = 200
MAX_HEIGHT = 200


class SSD1681(SSD1683):
    def __init__(
        self, name: str, class_name: str = "EPaperSSD1681", **defaults: Any
    ) -> None:
        super().__init__(name, class_name=class_name, **defaults)

    def validate_config(self, config: ConfigType) -> ConfigType:
        width, height = self.get_dimensions(config)
        if width > MAX_WIDTH or height > MAX_HEIGHT:
            raise cv.Invalid(
                f"{self.name} supports at most {MAX_WIDTH}x{MAX_HEIGHT} pixels",
                path=[CONF_DIMENSIONS],
            )
        return config


ssd1681 = SSD1681("ssd1681", width=200, height=200)

ssd1681.extend("goodisplay-gdey0154d67-1.54")
ssd1681.extend("waveshare-1.54in-v2")
