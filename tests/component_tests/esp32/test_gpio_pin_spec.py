"""Tests for the data driven ESP32 pin validators."""

import pytest

from esphome.components.const import CONF_HOLD_STATE
from esphome.components.esp32.gpio_common import (
    ESP32PinSpec,
    spi_psram_errors,
    unusable_errors,
)
import esphome.config_validation as cv
from esphome.const import (
    CONF_IGNORE_STRAPPING_WARNING,
    CONF_INPUT,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome.types import ConfigType

SPEC = ESP32PinSpec(
    max_pin=40,
    strapping_pins=frozenset({0, 3}),
    usb_jtag_pins=frozenset({3, 19}),
    error_pins={
        **spi_psram_errors("ESP32-X1", {26: "SPICS1"}),
        **unusable_errors("ESP32-X1", (22,)),
    },
    fatal_pins={29: "GPIO29 does not exist on ESP32-X1."},
    warning_pins={9: "GPIO9 might be used by flash."},
    input_only_pins=frozenset({34, 35}),
    input_only_label=" (34-35)",
    no_hold_pins=frozenset({20}),
)


def _conf(num: int, hold: bool = False, **mode: bool) -> ConfigType:
    return {
        CONF_NUMBER: num,
        CONF_MODE: {
            CONF_INPUT: False,
            CONF_OUTPUT: False,
            CONF_PULLUP: False,
            CONF_PULLDOWN: False,
            CONF_OPEN_DRAIN: False,
            **mode,
        },
        CONF_HOLD_STATE: hold,
    }


@pytest.mark.parametrize("num", [-1, 41])
def test_out_of_range_rejected_by_both(num: int) -> None:
    message = rf"Invalid pin number: {num} \(must be 0-40\)"
    with pytest.raises(cv.Invalid, match=message):
        SPEC.validate_gpio_pin(num)
    with pytest.raises(cv.Invalid, match=message):
        SPEC.validate_supports(_conf(num, input=True))


@pytest.mark.parametrize(
    ("num", "message"),
    [
        (
            26,
            (
                "This pin cannot be used on ESP32-X1s and is already used by the"
                " SPI/PSRAM interface (function: SPICS1)"
            ),
        ),
        (22, "The pin GPIO22 is not usable on ESP32-X1s."),
    ],
)
def test_error_pins_only_fail_pin_validation(num: int, message: str) -> None:
    with pytest.raises(cv.Invalid) as exc:
        SPEC.validate_gpio_pin(num)
    assert exc.value.msg == message
    SPEC.validate_supports(_conf(num, output=True))


def test_fatal_pins_fail_both_validators() -> None:
    for validate, value in (
        (SPEC.validate_gpio_pin, 29),
        (SPEC.validate_supports, _conf(29, input=True)),
    ):
        with pytest.raises(cv.Invalid, match="GPIO29 does not exist on ESP32-X1."):
            validate(value)


def test_warning_and_usb_jtag_pins(caplog: pytest.LogCaptureFixture) -> None:
    with caplog.at_level("WARNING"):
        assert SPEC.validate_gpio_pin(9) == 9
        assert SPEC.validate_gpio_pin(19) == 19
        assert SPEC.validate_gpio_pin(5) == 5
    assert caplog.messages == [
        "GPIO9 might be used by flash.",
        (
            "GPIO19 is used by the USB-Serial-JTAG interface."
            " Using this pin as GPIO will conflict with USB-Serial-JTAG."
        ),
    ]


@pytest.mark.parametrize(
    ("mode", "problem"),
    [
        (CONF_OUTPUT, "does not support output pin mode."),
        (CONF_PULLUP, "does not support pullups."),
        (CONF_PULLDOWN, "does not support pulldowns."),
    ],
)
def test_input_only_pins_reject_modes(mode: str, problem: str) -> None:
    with pytest.raises(cv.Invalid) as exc:
        SPEC.validate_supports(_conf(35, input=True, **{mode: True}))
    assert exc.value.msg == f"GPIO35 (34-35) {problem}"
    assert exc.value.path == [CONF_MODE, mode]
    # The same modes are fine on a normal pin
    SPEC.validate_supports(_conf(5, input=True, **{mode: True}))


@pytest.mark.parametrize(
    ("num", "message"),
    [
        (34, "GPIO34 (34-35) is input-only and cannot be held."),
        (20, "GPIO20 has no hold function."),
    ],
)
def test_pins_that_cannot_hold(num: int, message: str) -> None:
    SPEC.validate_supports(_conf(num, input=True))
    with pytest.raises(cv.Invalid) as exc:
        SPEC.validate_supports(_conf(num, hold=True, input=True))
    assert exc.value.msg == message
    assert exc.value.path == [CONF_HOLD_STATE]


def test_usb_jtag_hold_and_strapping_warnings(
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level("WARNING"):
        SPEC.validate_supports(_conf(3, hold=True, output=True))
    assert len(caplog.messages) == 2
    assert caplog.messages[0] == (
        "GPIO3 cannot hold at low level during wakeup from deep sleep."
    )
    assert caplog.messages[1].startswith("GPIO3 is a strapping pin")


def test_ignore_strapping_warning_on_normal_pin_rejected() -> None:
    conf = _conf(5, input=True)
    conf[CONF_IGNORE_STRAPPING_WARNING] = True
    with pytest.raises(cv.Invalid, match="GPIO5 is not a strapping pin"):
        SPEC.validate_supports(conf)
