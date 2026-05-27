import logging
from typing import Any

import esphome.config_validation as cv
from esphome.const import CONF_INPUT, CONF_MODE, CONF_NUMBER
from esphome.pins import check_strapping_pin

_ESP32S31_STRAPPING_PINS: set[int] = set()

_LOGGER = logging.getLogger(__name__)


def esp32_s31_validate_gpio_pin(value: int) -> int:
    if value < 0 or value > 61:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-61)")
    return value


def esp32_s31_validate_supports(value: dict[str, Any]) -> dict[str, Any]:
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]

    if num < 0 or num > 61:
        raise cv.Invalid(f"Invalid pin number: {num} (must be 0-61)")
    if is_input:
        # All ESP32 pins support input mode
        pass

    check_strapping_pin(value, _ESP32S31_STRAPPING_PINS, _LOGGER)
    return value
