"""Tests for the data driven ESP32 pin validators."""

import pytest

from esphome.components.const import CONF_HOLD_STATE
from esphome.components.esp32.const import (
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C5,
    VARIANT_ESP32C6,
    VARIANT_ESP32C61,
    VARIANT_ESP32H2,
    VARIANT_ESP32H4,
    VARIANT_ESP32H21,
    VARIANT_ESP32P4,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32S31,
)
from esphome.components.esp32.gpio import _esp32_validations
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


@pytest.mark.parametrize(
    ("num", "message"),
    [
        (-1, "Invalid pin number: -1 (must be 0-40)"),
        (41, "Invalid pin number: 41 (must be 0-40)"),
        (29, "GPIO29 does not exist on ESP32-X1."),
    ],
)
def test_rejected_by_both_validators(num: int, message: str) -> None:
    for validate, value in (
        (SPEC.validate_gpio_pin, num),
        (SPEC.validate_supports, _conf(num, input=True)),
    ):
        with pytest.raises(cv.Invalid) as exc:
            validate(value)
        assert exc.value.msg == message


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


_USB_JTAG = "is used by the USB-Serial-JTAG interface"


@pytest.mark.parametrize(
    ("variant", "num", "error", "warning"),
    [
        (VARIANT_ESP32, 4, None, None),
        (
            VARIANT_ESP32,
            6,
            "used by the flash interface (function: Flash Clock).",
            None,
        ),
        (VARIANT_ESP32, 9, None, "Pin 9 (9-10) might already be used by the flash"),
        (VARIANT_ESP32, 24, "The pin GPIO24 is not usable on ESP32s.", None),
        (VARIANT_ESP32, 40, "Invalid pin number: 40 (must be 0-39)", None),
        (VARIANT_ESP32C2, 8, None, None),
        (VARIANT_ESP32C2, 21, "Invalid pin number: 21 (must be 0-20)", None),
        (
            VARIANT_ESP32C3,
            12,
            "used on ESP32-C3s and is already used by the SPI/PSRAM",
            None,
        ),
        (VARIANT_ESP32C3, 18, None, _USB_JTAG),
        (
            VARIANT_ESP32C5,
            19,
            "ESP32-C5s and is already used by the SPI/PSRAM interface",
            None,
        ),
        (VARIANT_ESP32C5, 13, None, _USB_JTAG),
        (VARIANT_ESP32C6, 27, "SPI/PSRAM interface (function: VDD_SPI)", None),
        (VARIANT_ESP32C6, 12, None, _USB_JTAG),
        (
            VARIANT_ESP32C61,
            21,
            "used on ESP32-C61s and is already used by the SPI/PSRAM",
            None,
        ),
        (VARIANT_ESP32C61, 18, None, None),
        (
            VARIANT_ESP32H2,
            6,
            None,
            "GPIO6 is reserved for SPI Flash communication on some",
        ),
        (VARIANT_ESP32H2, 26, None, _USB_JTAG),
        (VARIANT_ESP32H2, 28, "Invalid pin number: 28 (must be 0-27)", None),
        (VARIANT_ESP32H21, 17, None, _USB_JTAG),
        (VARIANT_ESP32H4, 13, None, _USB_JTAG),
        (VARIANT_ESP32H4, 40, "Invalid pin number: 40 (must be 0-39)", None),
        (VARIANT_ESP32P4, 24, None, _USB_JTAG),
        (VARIANT_ESP32P4, 55, "Invalid pin number: 55 (must be 0-54)", None),
        (
            VARIANT_ESP32S2,
            26,
            "ESP32-S2s and is already used by the SPI/PSRAM interface",
            None,
        ),
        (VARIANT_ESP32S2, 22, "The pin GPIO22 is not usable on ESP32-S2s.", None),
        (VARIANT_ESP32S3, 33, None, None),
        (VARIANT_ESP32S3, 19, None, _USB_JTAG),
        (VARIANT_ESP32S3, 25, "The pin GPIO25 is not usable on ESP32-S3s.", None),
        (VARIANT_ESP32S3, 32, "SPI/PSRAM interface (function: SPID)", None),
        (
            VARIANT_ESP32S31,
            26,
            "GPIO26 is reserved for the SPI flash interface on ESP32-S31",
            None,
        ),
        (VARIANT_ESP32S31, 29, "GPIO29 does not exist on ESP32-S31.", None),
        (VARIANT_ESP32S31, 33, None, _USB_JTAG),
        (VARIANT_ESP32S31, 62, "Invalid pin number: 62 (must be 0-61)", None),
    ],
)
def test_variant_pin_validation(
    variant: str,
    num: int,
    error: str | None,
    warning: str | None,
    caplog: pytest.LogCaptureFixture,
) -> None:
    spec = _esp32_validations[variant]
    with caplog.at_level("WARNING"):
        if error is None:
            assert spec.validate_gpio_pin(num) == num
        else:
            with pytest.raises(cv.Invalid) as exc:
                spec.validate_gpio_pin(num)
            assert error in exc.value.msg
    if warning is None:
        assert not caplog.messages
    else:
        assert len(caplog.messages) == 1
        assert warning in caplog.messages[0]


@pytest.mark.parametrize(
    ("variant", "conf", "error"),
    [
        (
            VARIANT_ESP32,
            _conf(36, output=True),
            "GPIO36 (34-39) does not support output",
        ),
        (
            VARIANT_ESP32,
            _conf(39, pullup=True),
            "GPIO39 (34-39) does not support pullups.",
        ),
        (
            VARIANT_ESP32,
            _conf(34, hold=True),
            "GPIO34 (34-39) is input-only and cannot",
        ),
        (VARIANT_ESP32, _conf(20, hold=True), "GPIO20 has no hold function."),
        (VARIANT_ESP32, _conf(40), "Invalid pin number: 40 (must be 0-39)"),
        (
            VARIANT_ESP32S2,
            _conf(46, pulldown=True),
            "GPIO46 does not support pulldowns.",
        ),
        (
            VARIANT_ESP32S2,
            _conf(46, hold=True),
            "GPIO46 is input-only and cannot be held.",
        ),
        (VARIANT_ESP32S31, _conf(41), "GPIO41 does not exist on ESP32-S31."),
        (VARIANT_ESP32C2, _conf(21), "Invalid pin number: 21 (must be 0-20)"),
    ],
)
def test_variant_supports_errors(variant: str, conf: ConfigType, error: str) -> None:
    with pytest.raises(cv.Invalid) as exc:
        _esp32_validations[variant].validate_supports(conf)
    assert error in exc.value.msg


@pytest.mark.parametrize(
    ("variant", "conf", "warnings"),
    [
        (VARIANT_ESP32, _conf(0, output=True), ["GPIO0 is a strapping pin"]),
        (VARIANT_ESP32, _conf(35, input=True), []),
        (VARIANT_ESP32C3, _conf(18, hold=True), ["GPIO18 cannot hold at low level"]),
        (
            VARIANT_ESP32H4,
            _conf(13, hold=True),
            ["GPIO13 cannot hold at low level", "GPIO13 is a strapping pin"],
        ),
        (VARIANT_ESP32S3, _conf(46, input=True), ["GPIO46 is a strapping pin"]),
    ],
)
def test_variant_supports_warnings(
    variant: str,
    conf: ConfigType,
    warnings: list[str],
    caplog: pytest.LogCaptureFixture,
) -> None:
    with caplog.at_level("WARNING"):
        _esp32_validations[variant].validate_supports(conf)
    assert len(caplog.messages) == len(warnings)
    for message, expected in zip(caplog.messages, warnings, strict=True):
        assert message.startswith(expected)
