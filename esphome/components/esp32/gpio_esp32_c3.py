import logging

from esphome.const import (
    CONF_INPUT,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
import esphome.config_validation as cv

_LOGGER = logging.getLogger(__name__)


def esp32_c3_validate_gpio_pin(value):
    if value < 0 or value > 21:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-21)")
    return value


def esp32_c3_validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]
    is_output = mode[CONF_OUTPUT]
    is_open_drain = mode[CONF_OPEN_DRAIN]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]

    if num < 0 or num > 21:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-21)")

    if is_input:
        # All ESP32 pins support input mode
        pass
    if is_open_drain and not is_output:
        raise cv.Invalid(
            "Open-drain only works with output mode", [CONF_MODE, CONF_OPEN_DRAIN]
        )
    if is_pullup and num == 21:
        raise cv.Invalid(
            f"GPIO{num} does not support pullups.", [CONF_MODE, CONF_PULLUP]
        )
    if is_pulldown and num == 21:
        raise cv.Invalid(
            f"GPIO{num} does not support pulldowns.", [CONF_MODE, CONF_PULLDOWN]
        )

    return value
