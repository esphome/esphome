import esphome.config_validation as cv
from esphome.const import CONF_BUSY_PIN, CONF_INVERTED, CONF_RESET_DURATION
from esphome.types import ConfigType

from .jd79660 import JD79660


class Waveshare3P97InG(JD79660):
    source_files = ("epaper_spi_jd79660.cpp", "epaper_waveshare_3p97_g.cpp")

    def get_constructor_args(self, config: ConfigType) -> tuple:
        return ()

    def validate_config(self, config: ConfigType) -> ConfigType:
        if not config[CONF_BUSY_PIN][CONF_INVERTED]:
            raise cv.Invalid(
                f"{self.name} requires busy_pin.inverted: true; "
                "replace the bare GPIO with {number: GPIOxx, inverted: true}",
                path=[CONF_BUSY_PIN, CONF_INVERTED],
            )
        if CONF_RESET_DURATION in config:
            raise cv.Invalid(
                f"{self.name} uses a fixed 200 ms reset settle; "
                f"remove {CONF_RESET_DURATION}",
                path=[CONF_RESET_DURATION],
            )
        return config


# The panel only wakes from deep sleep with a hardware reset, so reset_pin is
# required. The reset settle is fixed at 200 ms per the vendor sample.
#
# Vendor init derived from the Waveshare 3.97inch E-Paper Display (G) sample.
# The 0x61 payload is deliberately 800x680; it must not be derived from the
# logical 800x480 display dimensions.
Waveshare3P97InG(
    "waveshare-3.97in-g",
    "EPaperWaveshare3P97InG",
    width=800,
    height=480,
    minimum_update_interval="30s",
    data_rate="10MHz",
    initsequence=(
        (0x00, 0x2B, 0x29),
        (0x06, 0x0F, 0x8B, 0x93, 0xC1),
        (0x50, 0x37),
        (0x30, 0x08),
        (0x61, 0x03, 0x20, 0x02, 0xA8),
        (0x62, 0x76, 0x76, 0x76, 0x5A, 0x9D, 0x8A, 0x76, 0x62),
        (0x65, 0x00, 0x00, 0x00, 0x00),
        (0xE0, 0x10),
        (0xE7, 0xA4),
        (0xE9, 0x01),
        (0xEF, 0x01),
        (0xF6, 0x20),
        (0xEF, 0x00),
        (0xE0, 0x12),
        (0xE6, 92),
        (0xA5, 0x00),
    ),
)
