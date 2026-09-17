import esphome.config_validation as cv
from esphome.const import CONF_DATA_RATE

from . import EpaperModel

CONF_BORDER_WAVEFORM = "border_waveform"


class SSD1677(EpaperModel):
    def __init__(
        self,
        name,
        class_name="EPaperMono",
        data_rate="20MHz",
        border_waveform=0x01,
        **defaults,
    ):
        defaults[CONF_DATA_RATE] = data_rate
        defaults[CONF_BORDER_WAVEFORM] = border_waveform
        defaults.setdefault("partial_update", True)
        super().__init__(name, class_name=class_name, **defaults)

    def get_config_options(self) -> dict:
        return {
            self.option(CONF_BORDER_WAVEFORM): cv.hex_uint8_t,
        }

    # fmt: off
    def get_init_sequence(self, config: dict):
        _width, height = self.get_dimensions(config)
        return (
            (0x18, 0x80),    # Select internal Temp sensor
            (0x0C, 0xAE, 0xC7, 0xC3, 0xC0, 0x80),  # inrush current level 2
            (0x01, (height - 1) % 256, (height - 1) // 256, 0x02),    # Set gate limit (number of rows-1)
            (0x3C, config[CONF_BORDER_WAVEFORM]),    # Set border waveform
            (0x11, 3),      # Set transform
        )


ssd1677 = SSD1677("ssd1677")


wave_4_26 = ssd1677.extend(
    "waveshare-4.26in",
    width=800,
    height=480,
    mirror_x=True,
)

wave_4_26.extend(
    "seeed-ee04-mono-4.26",
    cs_pin=44,
    dc_pin=10,
    reset_pin=38,
    busy_pin={
        "number": 4,
        "inverted": False,
        "mode": {
            "input": True,
            "pulldown": True,
        },
    },
)


ssd1677.extend(
    "waveshare-3.97in",
    width=800,
    height=480,
    mirror_x=True,
)

# Sticky - monochrome version
seeed_sticky = ssd1677.extend(
    "seeed-reterminal-sticky",
    width=800,
    height=480,
    mirror_x=True,
    enable_pin=47,
    cs_pin=15,
    dc_pin=16,
    reset_pin=17,
    busy_pin=18,
    data_rate="10MHz",
    requires={"psram"},
)

# Sticky - 4 level grayscale, no partial refresh support
seeed_sticky.extend(
    "seeed-reterminal-sticky-gray4",
    class_name="EPaperSSD1677Gray4",
    border_waveform=0x00,
    partial_update=False,
)
