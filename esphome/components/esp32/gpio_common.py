"""Pin checks shared by the ESP32 variant validators."""

from collections.abc import Iterable, Mapping, Set as AbstractSet
from dataclasses import dataclass, field
import logging
from logging import Logger
from typing import Any

from esphome.components.const import CONF_HOLD_STATE
import esphome.config_validation as cv
from esphome.const import (
    CONF_MODE,
    CONF_NUMBER,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome.pins import check_strapping_pin
from esphome.types import ConfigType

_LOGGER = logging.getLogger(__name__)

_INPUT_ONLY_REJECTED_MODES = (
    (CONF_OUTPUT, "does not support output pin mode."),
    (CONF_PULLUP, "does not support pullups."),
    (CONF_PULLDOWN, "does not support pulldowns."),
)


def check_usb_jtag_pin(
    num: int, usb_jtag_pins: AbstractSet[int], logger: Logger
) -> None:
    if num in usb_jtag_pins:
        logger.warning(
            "GPIO%d is used by the USB-Serial-JTAG interface."
            " Using this pin as GPIO will conflict with USB-Serial-JTAG.",
            num,
        )


def check_usb_jtag_hold(
    conf: dict[str, Any], usb_jtag_pins: AbstractSet[int], logger: Logger
) -> None:
    num = conf[CONF_NUMBER]
    if conf.get(CONF_HOLD_STATE) and num in usb_jtag_pins:
        logger.warning(
            "GPIO%d cannot hold at low level during wakeup from deep sleep.", num
        )


def spi_psram_errors(variant: str, functions: Mapping[int, str]) -> dict[int, str]:
    """Error messages for pins taken by the SPI flash/PSRAM interface."""
    return {
        num: f"This pin cannot be used on {variant}s and is already used by the"
        f" SPI/PSRAM interface (function: {function})"
        for num, function in functions.items()
    }


def unusable_errors(variant: str, pins: Iterable[int]) -> dict[int, str]:
    """Error messages for pins that are missing from the GPIO matrix."""
    return {num: f"The pin GPIO{num} is not usable on {variant}s." for num in pins}


@dataclass(frozen=True, kw_only=True)
class ESP32PinSpec:
    """Pin facts of one ESP32 variant, with the two validators built from them.

    `error_pins` fail pin validation, which `ignore_pin_validation_error` bypasses;
    `fatal_pins` fail both validators, so the option cannot bypass them.
    """

    max_pin: int
    strapping_pins: frozenset[int]
    usb_jtag_pins: frozenset[int] = frozenset()
    error_pins: Mapping[int, str] = field(default_factory=dict)
    fatal_pins: Mapping[int, str] = field(default_factory=dict)
    warning_pins: Mapping[int, str] = field(default_factory=dict)
    input_only_pins: frozenset[int] = frozenset()
    # Inserted after "GPIOn" in the input-only errors, e.g. " (34-39)"
    input_only_label: str = ""
    no_hold_pins: frozenset[int] = frozenset()

    def _check_pin_exists(self, num: int) -> None:
        if num < 0 or num > self.max_pin:
            raise cv.Invalid(f"Invalid pin number: {num} (must be 0-{self.max_pin})")
        if (message := self.fatal_pins.get(num)) is not None:
            raise cv.Invalid(message)

    def validate_gpio_pin(self, value: int) -> int:
        self._check_pin_exists(value)
        if (message := self.error_pins.get(value)) is not None:
            raise cv.Invalid(message)
        if (message := self.warning_pins.get(value)) is not None:
            _LOGGER.warning("%s", message)
        check_usb_jtag_pin(value, self.usb_jtag_pins, _LOGGER)
        return value

    def validate_supports(self, value: ConfigType) -> ConfigType:
        num = value[CONF_NUMBER]
        self._check_pin_exists(num)
        hold = value.get(CONF_HOLD_STATE)
        if num in self.input_only_pins:
            prefix = f"GPIO{num}{self.input_only_label}"
            mode = value[CONF_MODE]
            for key, problem in _INPUT_ONLY_REJECTED_MODES:
                if mode[key]:
                    raise cv.Invalid(f"{prefix} {problem}", [CONF_MODE, key])
            if hold:
                raise cv.Invalid(
                    f"{prefix} is input-only and cannot be held.", [CONF_HOLD_STATE]
                )
        if hold and num in self.no_hold_pins:
            raise cv.Invalid(f"GPIO{num} has no hold function.", [CONF_HOLD_STATE])
        check_usb_jtag_hold(value, self.usb_jtag_pins, _LOGGER)
        check_strapping_pin(value, self.strapping_pins, _LOGGER)
        return value
