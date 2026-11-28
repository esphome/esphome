"""Tests for the M5Stack PM1 pin options and final validation."""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

import pytest

from esphome import final_validate
from esphome.components import i2c
from esphome.components.m5stack_pm1 import (
    CONF_FAST_MODE,
    CONF_INTERRUPT_GPIO,
    CONF_M5STACK_PM1_ID,
    CONF_WAKEUP,
    CONF_WAKEUP_EDGE,
    CONFIG_SCHEMA,
    FINAL_VALIDATE_SCHEMA,
    M5STACK_PM1_PIN_SCHEMA,
    M5StackPM1HubPins,
    _hub_pins,
    validate_pin_usage,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_FREQUENCY,
    CONF_I2C_ID,
    CONF_ID,
    CONF_INPUT,
    CONF_INTERRUPT,
    CONF_INTERRUPT_PIN,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome.core import ID
from esphome.types import ConfigType

HUB_ID = "pm1"
BUS_ID = "i2c_bus"


def _hub_config(hub_id: str = HUB_ID, **options: object) -> ConfigType:
    config: ConfigType = {CONF_ID: ID(hub_id, is_declaration=True)}
    config.update(options)
    return config


def _pin_config(number: int, **options: object) -> ConfigType:
    config: ConfigType = {
        CONF_M5STACK_PM1_ID: ID(HUB_ID),
        CONF_NUMBER: number,
        CONF_MODE: {CONF_INPUT: True},
    }
    config.update(options)
    return config


def _pin_usage(number: int, **options: object) -> ConfigType:
    return {CONF_NUMBER: number, CONF_INTERRUPT: False, **options}


@pytest.mark.parametrize(
    ("fast_mode", "frequency", "error"),
    [
        (True, 400000.0, False),
        (True, 50000.0, True),
        (True, 100000.0, True),
        (True, 200000.0, True),
        (False, 50000.0, False),
    ],
)
def test_fast_mode_needs_a_400k_bus(
    set_component_config: Callable[[str, Any], None],
    fast_mode: bool,
    frequency: float,
    error: bool,
) -> None:
    bus_id = ID(BUS_ID, is_declaration=True, type=i2c.IDFI2CBus)
    set_component_config("i2c", [{CONF_ID: bus_id, CONF_FREQUENCY: frequency}])
    final_validate.full_config.get().declare_ids.append((bus_id, ["i2c", 0, CONF_ID]))
    config = _hub_config(**{CONF_I2C_ID: ID(BUS_ID), CONF_FAST_MODE: fast_mode})

    if error:
        with pytest.raises(cv.Invalid, match="requires a frequency between 400kHz"):
            FINAL_VALIDATE_SCHEMA(config)
    else:
        FINAL_VALIDATE_SCHEMA(config)


def test_fast_mode_defaults_to_off() -> None:
    assert CONFIG_SCHEMA({})[CONF_FAST_MODE] is False


@pytest.mark.parametrize(
    ("wakeup_edge", "explicit_pull", "expected_pull"),
    [
        ("rising", None, CONF_PULLDOWN),
        ("falling", None, CONF_PULLUP),
        (None, None, CONF_PULLUP),
        # An explicit pull is kept even when it matches the wake edge.
        ("falling", CONF_PULLDOWN, CONF_PULLDOWN),
        ("rising", CONF_PULLUP, CONF_PULLUP),
    ],
)
def test_wakeup_pull(
    wakeup_edge: str | None, explicit_pull: str | None, expected_pull: str
) -> None:
    """The default pull keeps the line at the level the edge starts from."""
    options: dict[str, object] = {CONF_WAKEUP: True}
    if wakeup_edge is not None:
        options[CONF_WAKEUP_EDGE] = wakeup_edge
    config = _pin_config(2, **options)
    if explicit_pull is not None:
        config[CONF_MODE][explicit_pull] = True

    result = M5STACK_PM1_PIN_SCHEMA(config)

    assert result[CONF_MODE][expected_pull] is True
    other_pull = CONF_PULLUP if expected_pull == CONF_PULLDOWN else CONF_PULLDOWN
    assert result[CONF_MODE][other_pull] is False


@pytest.mark.parametrize(
    ("first", "second", "second_hub", "error"),
    [
        (0, 2, HUB_ID, True),
        (3, 4, HUB_ID, True),
        (0, 3, HUB_ID, False),
        (2, 4, HUB_ID, False),
        (0, 2, "other", False),
    ],
)
def test_wake_pins_sharing_an_interrupt_line(
    first: int, second: int, second_hub: str, error: bool
) -> None:
    validate_pin_usage(_pin_usage(first, wakeup=True), _hub_config())

    if error:
        with pytest.raises(cv.Invalid, match=f"GPIO{first} and GPIO{second}"):
            validate_pin_usage(_pin_usage(second, wakeup=True), _hub_config(second_hub))
    else:
        validate_pin_usage(_pin_usage(second, wakeup=True), _hub_config(second_hub))


def test_pin_usage_records_wake_and_interrupt_bits() -> None:
    hub = _hub_config(**{CONF_INTERRUPT_PIN: {}, CONF_INTERRUPT_GPIO: 4})
    validate_pin_usage(_pin_usage(0, interrupt=True), hub)
    validate_pin_usage(_pin_usage(2, wakeup=True, wakeup_edge="RISING"), hub)
    validate_pin_usage(_pin_usage(3, wakeup=True, wakeup_edge="FALLING"), hub)

    assert _hub_pins(HUB_ID) == M5StackPM1HubPins(
        wake_enable=0b01100, wake_rising=0b00100, interrupt=0b00001
    )


@pytest.mark.parametrize(
    ("number", "options", "message"),
    [
        (
            3,
            {CONF_MODE: {CONF_INPUT: True, CONF_OUTPUT: True}},
            "Mode must be either input or output",
        ),
        (
            3,
            {CONF_MODE: {CONF_INPUT: True, CONF_PULLUP: True, CONF_PULLDOWN: True}},
            "Only one of pullup and pulldown",
        ),
        (
            3,
            {CONF_MODE: {CONF_OUTPUT: True, CONF_PULLUP: True}},
            "Pull resistors are only supported in input mode",
        ),
        (
            3,
            {CONF_MODE: {CONF_INPUT: True, CONF_OPEN_DRAIN: True}},
            "Open-drain is only supported in output mode",
        ),
        (
            3,
            {CONF_MODE: {CONF_OUTPUT: True}, CONF_WAKEUP: True},
            "GPIO wakeup is only supported in input mode",
        ),
        (
            3,
            {CONF_MODE: {CONF_OUTPUT: True}, CONF_INTERRUPT: True},
            "GPIO interrupt is only supported in input mode",
        ),
        (1, {CONF_WAKEUP: True}, "GPIO1 does not support wakeup"),
        (3, {CONF_WAKEUP_EDGE: "rising"}, "wakeup_edge requires wakeup"),
        (5, {}, "must be at most 4"),
    ],
)
def test_invalid_pin_configs_are_rejected(
    number: int, options: ConfigType, message: str
) -> None:
    with pytest.raises(cv.Invalid, match=message):
        M5STACK_PM1_PIN_SCHEMA(_pin_config(number, **options))


@pytest.mark.parametrize(
    ("pin", "message"),
    [
        (_pin_usage(0, interrupt=True), "requires 'interrupt_pin'"),
        (_pin_usage(4), "is used as 'interrupt_gpio'"),
    ],
)
def test_invalid_pin_usage_is_rejected(pin: ConfigType, message: str) -> None:
    with pytest.raises(cv.Invalid, match=message):
        validate_pin_usage(pin, _hub_config(**{CONF_INTERRUPT_GPIO: 4}))


def test_interrupt_gpio_requires_interrupt_pin() -> None:
    with pytest.raises(cv.Invalid, match="either none or all of"):
        CONFIG_SCHEMA({CONF_INTERRUPT_GPIO: 4})
