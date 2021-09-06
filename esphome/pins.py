from esphome.components.esp32 import CONF_PULLUP
import logging

import esphome.config_validation as cv
from esphome.const import CONF_ANALOG, CONF_INPUT, CONF_MODE, CONF_NUMBER, CONF_OUTPUT
from esphome.core import CORE
from esphome.util import SimpleRegistry
from esphome import boards

_LOGGER = logging.getLogger(__name__)


PIN_SCHEMA_REGISTRY = SimpleRegistry()


def _set_mode(value, default_mode):
    if CONF_MODE in value:
        return value
    return {**value, CONF_MODE: default_mode}


def _schema_creator(default_mode, internal: bool = False):
    def validator(value):
        if not isinstance(value, dict):
            return validator({CONF_NUMBER: value})
        value = _set_mode(value, default_mode)
        if not internal:
            for key, entry in PIN_SCHEMA_REGISTRY.items():
                if key != "default" and key in value:
                    return entry[1](value)
        return PIN_SCHEMA_REGISTRY["default"][1](value)

    return validator


gpio_output_pin_schema = _schema_creator(
    {
        CONF_OUTPUT: True,
    }
)
gpio_input_pin_schema = _schema_creator(
    {
        CONF_INPUT: True,
    }
)
gpio_input_pullup_pin_schema = _schema_creator(
    {
        CONF_INPUT: True,
        CONF_PULLUP: True,
    }
)
internal_gpio_output_pin_schema = _schema_creator(
    {
        CONF_OUTPUT: True,
    },
    internal=True,
)
internal_gpio_input_pin_schema = _schema_creator(
    {
        CONF_INPUT: True,
    },
    internal=True,
)
internal_gpio_input_pullup_pin_schema = _schema_creator(
    {
        CONF_INPUT: True,
        CONF_PULLUP: True,
    },
    internal=True,
)
internal_gpio_analog_pin_schema = _schema_creator(
    {
        CONF_ANALOG: True,
    },
    internal=True,
)
