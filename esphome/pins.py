import operator
from functools import reduce

from esphome.const import (
    CONF_INPUT,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome.util import SimpleRegistry
from esphome.core import CORE


PIN_SCHEMA_REGISTRY = SimpleRegistry()


def _set_mode(value, default_mode):
    import esphome.config_validation as cv

    if CONF_MODE not in value:
        return {**value, CONF_MODE: default_mode}
    mode = value[CONF_MODE]
    if not isinstance(mode, str):
        return value
    # mode is a string, try parsing it like arduino pin modes
    PIN_MODES = {
        "INPUT": {
            CONF_INPUT: True,
        },
        "OUTPUT": {
            CONF_OUTPUT: True,
        },
        "INPUT_PULLUP": {
            CONF_INPUT: True,
            CONF_PULLUP: True,
        },
        "OUTPUT_OPEN_DRAIN": {
            CONF_OUTPUT: True,
            CONF_OPEN_DRAIN: True,
        },
        "INPUT_PULLDOWN_16": {
            CONF_INPUT: True,
            CONF_PULLDOWN: True,
        },
        "INPUT_PULLDOWN": {
            CONF_INPUT: True,
            CONF_PULLDOWN: True,
        },
    }
    if mode.upper() not in PIN_MODES:
        raise cv.Invalid(f"Unknown pin mode {mode}", [CONF_MODE])
    return {**value, CONF_MODE: PIN_MODES[mode.upper()]}


def _schema_creator(default_mode, internal: bool = False):
    def validator(value):
        if not isinstance(value, dict):
            return validator({CONF_NUMBER: value})
        value = _set_mode(value, default_mode)
        if not internal:
            for key, entry in PIN_SCHEMA_REGISTRY.items():
                if key != CORE.target_platform and key in value:
                    return entry[1](value)
        return PIN_SCHEMA_REGISTRY[CORE.target_platform][1](value)

    return validator


def _internal_number_creator(mode):
    def validator(value):
        value_d = {CONF_NUMBER: value}
        value_d = _set_mode(value_d, mode)
        return PIN_SCHEMA_REGISTRY[CORE.target_platform][1](value_d)[CONF_NUMBER]

    return validator


def gpio_flags_expr(mode):
    """Convert the given mode dict to a gpio Flags expression"""
    import esphome.codegen as cg

    FLAGS_MAPPING = {
        CONF_INPUT: cg.gpio_Flags.FLAG_INPUT,
        CONF_OUTPUT: cg.gpio_Flags.FLAG_OUTPUT,
        CONF_OPEN_DRAIN: cg.gpio_Flags.FLAG_OPEN_DRAIN,
        CONF_PULLUP: cg.gpio_Flags.FLAG_PULLUP,
        CONF_PULLDOWN: cg.gpio_Flags.FLAG_PULLDOWN,
    }
    active_flags = [v for k, v in FLAGS_MAPPING.items() if mode.get(k)]
    if not active_flags:
        return cg.gpio_Flags.FLAG_NONE

    return reduce(operator.or_, active_flags)


gpio_pin_schema = _schema_creator
internal_gpio_pin_number = _internal_number_creator
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
internal_gpio_output_pin_number = _internal_number_creator({CONF_OUTPUT: True})
internal_gpio_input_pin_schema = _schema_creator(
    {
        CONF_INPUT: True,
    },
    internal=True,
)
internal_gpio_input_pin_number = _internal_number_creator({CONF_INPUT: True})
internal_gpio_input_pullup_pin_schema = _schema_creator(
    {
        CONF_INPUT: True,
        CONF_PULLUP: True,
    },
    internal=True,
)
internal_gpio_input_pullup_pin_number = _internal_number_creator(
    {
        CONF_INPUT: True,
        CONF_PULLUP: True,
    }
)
