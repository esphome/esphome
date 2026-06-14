from esphome import pins
import esphome.config_validation as cv
from esphome.const import CONF_DC_PIN

from ..display import CONF_CS_SLAVE_PIN
from . import EpaperModel


def _inverted_busy_schema(config):
    """Like gpio_input_pin_schema but defaults inverted to True."""
    if not isinstance(config, dict):
        config = {"number": config, "inverted": True}
    elif "inverted" not in config:
        config = {**config, "inverted": True}
    return pins.gpio_input_pin_schema(config)


class SpectraE6DualCSModel(EpaperModel):
    """Model class for Spectra E6 displays with dual chip-select (master + slave)."""

    def __init__(self, name, **defaults):
        super().__init__(name, "EPaperSpectraE6DualCS", **defaults)

    def option(self, name, fallback=cv.UNDEFINED):
        if name == CONF_CS_SLAVE_PIN:
            return cv.Required(name)
        if name == CONF_DC_PIN:
            # This panel has no D/C line — command/data is framed by CS. Make it optional.
            return cv.Optional(name)
        return super().option(name, fallback)

    def get_busy_pin_schema(self):
        return _inverted_busy_schema

    def get_init_sequence(self, config):
        return ()  # Init is hardcoded in C++ class


SpectraE6DualCSModel(
    "13.3in-Spectra-E6",
    width=1200,
    height=1600,
    data_rate="2MHz",
    minimum_update_interval="30s",
)
