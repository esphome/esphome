"""External component that draws into a display from outside its lambda or pages."""

from esphome.components import display
import esphome.config_validation as cv
from esphome.const import CONF_DISPLAY_ID
from esphome.types import ConfigType

DEPENDENCIES = ["display"]
DOMAIN = "fake_display_writer"


def _register_writer(config: ConfigType) -> ConfigType:
    display.register_external_writer(config[CONF_DISPLAY_ID], DOMAIN)
    return config


CONFIG_SCHEMA = cv.All(
    cv.Schema({cv.GenerateID(CONF_DISPLAY_ID): cv.use_id(display.Display)}),
    _register_writer,
)
