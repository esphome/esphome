import logging
from typing import Any

import esphome.config_validation as cv
from esphome.const import CONF_INPUT, CONF_MODE, CONF_NUMBER
from esphome.pins import check_strapping_pin

# Partial set from the ESP-IDF / esptool boot-mode docs:
# https://docs.espressif.com/projects/esptool/en/latest/esp32h21/advanced-topics/boot-mode-selection.html
# The full list awaits the ESP32-H21 datasheet's "Strapping Pins" section.
_ESP32H21_STRAPPING_PINS: set[int] = {13, 14}

_LOGGER = logging.getLogger(__name__)


def esp32_h21_validate_gpio_pin(value: int) -> int:
    if value < 0 or value > 25:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-25)")
    return value


def esp32_h21_validate_supports(value: dict[str, Any]) -> dict[str, Any]:
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]

    if num < 0 or num > 25:
        raise cv.Invalid(f"Invalid pin number: {num} (must be 0-25)")
    if is_input:
        # All ESP32 pins support input mode
        pass

    check_strapping_pin(value, _ESP32H21_STRAPPING_PINS, _LOGGER)
    return value
