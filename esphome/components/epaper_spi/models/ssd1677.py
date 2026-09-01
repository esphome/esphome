import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_DATA_RATE

from . import EpaperModel

# A component-local constant (esphome/const.py is frozen); it8951/display.py also defines this.
CONF_GRAYSCALE = "grayscale"


class SSD1677(EpaperModel):
    def __init__(
        self,
        name,
        class_name="EPaperMono",
        data_rate="20MHz",
        supports_grayscale=False,
        **defaults,
    ):
        defaults[CONF_DATA_RATE] = data_rate
        self.supports_grayscale = supports_grayscale
        super().__init__(name, class_name, **defaults)

    # fmt: off
    def get_init_sequence(self, config: dict):
        _width, height = self.get_dimensions(config)
        return (
            (0x18, 0x80),    # Select internal Temp sensor
            (0x0C, 0xAE, 0xC7, 0xC3, 0xC0, 0x80),  # inrush current level 2
            (0x01, (height - 1) % 256, (height - 1) // 256, 0x02),    # Set gate limit (number of rows-1)
            (0x3C, 0x01),    # Set border waveform
            (0x11, 3),      # Set transform
        )

    def get_config_options(self) -> dict:
        if not self.supports_grayscale:
            return {}
        # Default false: this option is additive, existing 1bpp configs must keep rendering
        # exactly as before if they don't opt in.
        return {cv.Optional(CONF_GRAYSCALE, default=False): cv.boolean}

    async def to_code(self, var, config: dict) -> dict:
        if self.supports_grayscale:
            cg.add(var.set_grayscale(config[CONF_GRAYSCALE]))
        return config


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

# Grayscale (4-level, factory OTP waveform) is only offered here: the register values it relies on
# are confirmed against Seeed's own driver and the stock reTerminal Sticky firmware specifically,
# and are not verified for any other SSD1677 board's glass.
ssd1677.extend(
    "seeed-reterminal-sticky",
    class_name="EPaperSSD1677",
    supports_grayscale=True,
    width=800,
    height=480,
    mirror_x=True,
    enable_pin=47,
    cs_pin=15,
    dc_pin=16,
    reset_pin=17,
    busy_pin=18,
    data_rate="10MHz",
)
