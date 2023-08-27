import logging

from esphome.const import CONF_INPUT, CONF_MODE, CONF_NUMBER

import esphome.config_validation as cv

_ESP32C2_STRAPPING_PINS = {8, 9}

_LOGGER = logging.getLogger(__name__)


def esp32_c2_validate_gpio_pin(value):
    if value < 0 or value > 20:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-20)")
    if value in _ESP32C2_STRAPPING_PINS:
        _LOGGER.warning(
            "GPIO%d is a Strapping PIN and should be avoided.\n"
            "Attaching external pullup/down resistors to strapping pins can cause unexpected failures.\n"
            "See https://esphome.io/guides/faq.html#why-am-i-getting-a-warning-about-strapping-pins",
            value,
        )

    return value


def esp32_c2_validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]

    if num < 0 or num > 20:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-20)")

    if is_input:
        # All ESP32 pins support input mode
        pass
    return value
