from esphome.const import (
    CONF_NUMBER,
)

import esphome.config_validation as cv


def esp32_h2_validate_gpio_pin(value):
    # ESP32-H2 not yet supported
    raise cv.Invalid("ESP32-H2 isn't supported yet")


def esp32_h2_validate_supports(value):
    # ESP32-H2 not yet supported
    if value > 0:
        raise cv.Invalid("ESP32-H2 isn't supported yet")

    num = value[CONF_NUMBER]
    if num < 0 or num > 21:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-21)")
    return value
