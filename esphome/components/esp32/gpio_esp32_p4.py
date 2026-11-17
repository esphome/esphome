import esphome.config_validation as cv
from esphome.const import CONF_SCL, CONF_SDA

from .gpio_common import ESP32PinSpec

# https://documentation.espressif.com/esp32-p4-chip-revision-v1.3_datasheet_en.pdf
_ESP32P4_LP_PINS = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15}

ESP32_P4_PIN_SPEC = ESP32PinSpec(
    max_pin=54,
    strapping_pins=frozenset({34, 35, 36, 37, 38}),
    usb_jtag_pins=frozenset({24, 25}),
)


def esp32_p4_validate_lp_i2c(value):
    if (
        int(value[CONF_SDA]) not in _ESP32P4_LP_PINS
        or int(value[CONF_SCL]) not in _ESP32P4_LP_PINS
    ):
        raise cv.Invalid(
            f"Low power i2c interface for ESP32-P4 is only supported on low power interface GPIO{min(_ESP32P4_LP_PINS)} - GPIO{max(_ESP32P4_LP_PINS)}"
        )
    return value
