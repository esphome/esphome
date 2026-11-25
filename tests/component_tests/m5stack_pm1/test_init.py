"""Tests for the M5Stack PM1 pin options and final validation."""

from __future__ import annotations

from collections.abc import Callable
from typing import Any

import pytest

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
    validate_interrupt_config,
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


def _hub_config(*, fast_mode: bool = False) -> ConfigType:
    return {
        CONF_ID: ID(HUB_ID, is_declaration=True),
        CONF_I2C_ID: ID(BUS_ID),
        CONF_FAST_MODE: fast_mode,
    }


def _bus(frequency: int) -> ConfigType:
    return {
        CONF_ID: ID(BUS_ID, is_declaration=True, type=i2c.IDFI2CBus),
        CONF_FREQUENCY: frequency,
    }


def _set_i2c_bus(
    set_component_config: Callable[[str, Any], None], frequency: int = 100000
) -> None:
    set_component_config("i2c", [_bus(frequency)])


def test_fast_mode_with_a_400k_bus_is_accepted(
    set_component_config: Callable[[str, Any], None],
) -> None:
    _set_i2c_bus(set_component_config, 400000)

    FINAL_VALIDATE_SCHEMA(_hub_config(fast_mode=True))


@pytest.mark.parametrize("frequency", [50000, 100000, 200000])
def test_fast_mode_with_a_slower_bus_is_rejected(
    set_component_config: Callable[[str, Any], None], frequency: int
) -> None:
    _set_i2c_bus(set_component_config, frequency)

    with pytest.raises(cv.Invalid, match="requires the I2C bus to run at 400kHz"):
        FINAL_VALIDATE_SCHEMA(_hub_config(fast_mode=True))


def test_standard_mode_accepts_any_bus_frequency(
    set_component_config: Callable[[str, Any], None],
) -> None:
    _set_i2c_bus(set_component_config, 50000)

    FINAL_VALIDATE_SCHEMA(_hub_config())


def test_fast_mode_defaults_to_off() -> None:
    assert CONFIG_SCHEMA({})[CONF_FAST_MODE] is False


@pytest.mark.parametrize(
    ("wakeup_edge", "expected_pull"),
    [
        ("rising", CONF_PULLDOWN),
        ("falling", CONF_PULLUP),
        (None, CONF_PULLUP),
    ],
)
def test_wakeup_default_pull_opposes_edge(
    wakeup_edge: str | None, expected_pull: str
) -> None:
    """The default pull keeps the line at the level the edge starts from."""
    options: dict[str, object] = {CONF_WAKEUP: True}
    if wakeup_edge is not None:
        options[CONF_WAKEUP_EDGE] = wakeup_edge

    result = M5STACK_PM1_PIN_SCHEMA(_pin_config(2, **options))

    assert result[CONF_MODE][expected_pull] is True
    other_pull = CONF_PULLUP if expected_pull == CONF_PULLDOWN else CONF_PULLDOWN
    assert result[CONF_MODE][other_pull] is False


def test_wakeup_keeps_explicit_pulldown() -> None:
    config = _pin_config(2, wakeup=True, wakeup_edge="falling")
    config[CONF_MODE][CONF_PULLDOWN] = True

    result = M5STACK_PM1_PIN_SCHEMA(config)

    assert result[CONF_MODE][CONF_PULLDOWN] is True
    assert result[CONF_MODE][CONF_PULLUP] is False


def test_wakeup_keeps_explicit_pull() -> None:
    config = _pin_config(2, wakeup=True, wakeup_edge="rising")
    config[CONF_MODE][CONF_PULLUP] = True

    result = M5STACK_PM1_PIN_SCHEMA(config)

    assert result[CONF_MODE][CONF_PULLUP] is True
    assert result[CONF_MODE][CONF_PULLDOWN] is False


@pytest.mark.parametrize(("first", "second"), [(0, 2), (3, 4)])
def test_wake_pins_sharing_an_interrupt_line_are_rejected(
    set_component_config: Callable[[str, Any], None], first: int, second: int
) -> None:
    _set_i2c_bus(set_component_config)
    set_component_config(
        "binary_sensor",
        [
            {CONF_M5STACK_PM1_ID: ID(HUB_ID), CONF_NUMBER: first, CONF_WAKEUP: True},
            {CONF_M5STACK_PM1_ID: ID(HUB_ID), CONF_NUMBER: second, CONF_WAKEUP: True},
        ],
    )

    with pytest.raises(cv.Invalid, match=f"GPIO{first} and GPIO{second}"):
        FINAL_VALIDATE_SCHEMA(_hub_config())


@pytest.mark.parametrize(("first", "second"), [(0, 3), (2, 4)])
def test_wake_pins_on_different_lines_are_accepted(
    set_component_config: Callable[[str, Any], None], first: int, second: int
) -> None:
    _set_i2c_bus(set_component_config)
    set_component_config(
        "binary_sensor",
        [
            {CONF_M5STACK_PM1_ID: ID(HUB_ID), CONF_NUMBER: first, CONF_WAKEUP: True},
            {CONF_M5STACK_PM1_ID: ID(HUB_ID), CONF_NUMBER: second, CONF_WAKEUP: True},
        ],
    )

    FINAL_VALIDATE_SCHEMA(_hub_config())


def test_wake_pins_on_another_hub_are_ignored(
    set_component_config: Callable[[str, Any], None],
) -> None:
    _set_i2c_bus(set_component_config)
    set_component_config(
        "binary_sensor",
        [
            {CONF_M5STACK_PM1_ID: ID(HUB_ID), CONF_NUMBER: 0, CONF_WAKEUP: True},
            {CONF_M5STACK_PM1_ID: ID("other"), CONF_NUMBER: 2, CONF_WAKEUP: True},
        ],
    )

    FINAL_VALIDATE_SCHEMA(_hub_config())


def _pin_config(number: int, **options: object) -> ConfigType:
    config: ConfigType = {
        CONF_M5STACK_PM1_ID: ID(HUB_ID),
        CONF_NUMBER: number,
        CONF_MODE: {CONF_INPUT: True},
    }
    config.update(options)
    return config


@pytest.mark.parametrize(
    ("mode", "message"),
    [
        ({CONF_INPUT: True, CONF_OUTPUT: True}, "Mode must be either input or output"),
        (
            {CONF_INPUT: True, CONF_PULLUP: True, CONF_PULLDOWN: True},
            "Only one of pullup and pulldown",
        ),
        (
            {CONF_OUTPUT: True, CONF_PULLUP: True},
            "Pull resistors are only supported in input mode",
        ),
        (
            {CONF_INPUT: True, CONF_OPEN_DRAIN: True},
            "Open-drain is only supported in output mode",
        ),
    ],
)
def test_conflicting_pin_modes_are_rejected(
    mode: dict[str, bool], message: str
) -> None:
    with pytest.raises(cv.Invalid, match=message):
        M5STACK_PM1_PIN_SCHEMA(_pin_config(3, mode=mode))


def test_output_pin_cannot_wake() -> None:
    with pytest.raises(cv.Invalid, match="GPIO wakeup is only supported in input mode"):
        M5STACK_PM1_PIN_SCHEMA(_pin_config(3, mode={CONF_OUTPUT: True}, wakeup=True))


def test_output_pin_cannot_interrupt() -> None:
    with pytest.raises(
        cv.Invalid, match="GPIO interrupt is only supported in input mode"
    ):
        M5STACK_PM1_PIN_SCHEMA(_pin_config(3, mode={CONF_OUTPUT: True}, interrupt=True))


def test_gpio1_cannot_wake() -> None:
    with pytest.raises(cv.Invalid, match="GPIO1 does not support wakeup"):
        M5STACK_PM1_PIN_SCHEMA(_pin_config(1, wakeup=True))


def test_wakeup_edge_requires_wakeup() -> None:
    with pytest.raises(cv.Invalid, match="wakeup_edge requires wakeup"):
        M5STACK_PM1_PIN_SCHEMA(_pin_config(3, wakeup_edge="rising"))


def test_interrupt_without_interrupt_pin_is_rejected() -> None:
    with pytest.raises(cv.Invalid, match="requires 'interrupt_pin'"):
        validate_pin_usage(
            {CONF_NUMBER: 0, CONF_INTERRUPT: True},
            {CONF_INTERRUPT_GPIO: 4},
        )


def test_reserved_interrupt_gpio_cannot_be_used_as_a_pin() -> None:
    with pytest.raises(cv.Invalid, match="is used as 'interrupt_gpio'"):
        validate_pin_usage(
            {CONF_NUMBER: 4, CONF_INTERRUPT: False},
            {CONF_INTERRUPT_GPIO: 4},
        )


@pytest.mark.parametrize(
    "config",
    [{CONF_INTERRUPT_PIN: {"number": 2}}, {CONF_INTERRUPT_GPIO: 4}],
)
def test_interrupt_pin_and_gpio_must_be_configured_together(
    config: ConfigType,
) -> None:
    with pytest.raises(cv.Invalid, match="must be configured together"):
        validate_interrupt_config(config)


def test_interrupt_pin_and_gpio_together_are_accepted() -> None:
    validate_interrupt_config(
        {CONF_INTERRUPT_PIN: {"number": 2}, CONF_INTERRUPT_GPIO: 4}
    )


def test_pin_number_above_the_last_gpio_is_rejected() -> None:
    with pytest.raises(cv.Invalid):
        M5STACK_PM1_PIN_SCHEMA(_pin_config(5))
