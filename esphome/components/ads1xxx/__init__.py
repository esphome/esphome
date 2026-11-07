import esphome.config_validation as cv
from esphome.const import CONF_GAIN, CONF_MULTIPLEXER
from esphome.types import ConfigType

CODEOWNERS = ["@bdraco"]
DOMAIN = "ads1xxx"

MUX = {
    name: i
    for i, name in enumerate(
        ("A0_A1", "A0_A3", "A1_A3", "A2_A3", "A0_GND", "A1_GND", "A2_GND", "A3_GND")
    )
}
GAIN = {
    name: i
    for i, name in enumerate(("6.144", "4.096", "2.048", "1.024", "0.512", "0.256"))
}

MUX_GAIN_SCHEMA = {
    cv.Required(CONF_MULTIPLEXER): cv.enum(MUX, upper=True, space="_"),
    cv.Required(CONF_GAIN): cv.enum(GAIN, string=True),
}


def mux_gain_bits(config: ConfigType) -> int:
    """Return the multiplexer and gain fields of the config register."""
    return config[CONF_MULTIPLEXER].enum_value << 12 | config[CONF_GAIN].enum_value << 9
