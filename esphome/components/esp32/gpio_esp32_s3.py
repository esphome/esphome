import logging
from typing import Any

import esphome.config_validation as cv
from esphome.const import (
    CONF_DISABLED,
    CONF_INPUT,
    CONF_MODE,
    CONF_NUMBER,
    PLATFORM_ESP32,
)
from esphome.pins import PIN_SCHEMA_REGISTRY, check_strapping_pin
from esphome.types import ConfigType

_ESP32S3_SPI_PSRAM_PINS = {
    26: "SPICS1",
    27: "SPIHD",
    28: "SPIWP",
    29: "SPICS0",
    30: "SPICLK",
    31: "SPIQ",
    32: "SPID",
}

_ESP32S3R8_PSRAM_PINS = {
    33: "SPIIO4",
    34: "SPIIO5",
    35: "SPIIO6",
    36: "SPIIO7",
    37: "SPIDQS",
}

_ESP32S3_USB_JTAG_PINS = {19, 20}

_ESP32S3_STRAPPING_PINS = {0, 3, 45, 46}

_LOGGER = logging.getLogger(__name__)


def esp32_s3_validate_gpio_pin(value: int) -> int:
    if value < 0 or value > 48:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-48)")

    if value in _ESP32S3_SPI_PSRAM_PINS:
        raise cv.Invalid(
            f"This pin cannot be used on ESP32-S3s and is already used by the SPI/PSRAM interface(function: {_ESP32S3_SPI_PSRAM_PINS[value]})"
        )
    # GPIO33-37 (_ESP32S3R8_PSRAM_PINS) are only taken by the PSRAM interface in
    # octal mode -- whether that applies isn't known here, so the warning is
    # deferred to final_validate_pins() in gpio.py once the PSRAM mode is resolved.

    if value in (22, 23, 24, 25):
        # These pins are not exposed in GPIO mux (reason unknown)
        # but they're missing from IO_MUX list in datasheet
        raise cv.Invalid(f"The pin GPIO{value} is not usable on ESP32-S3s.")
    if value in _ESP32S3_USB_JTAG_PINS:
        _LOGGER.warning(
            "GPIO%d is used by the USB-Serial-JTAG interface."
            " Using this pin as GPIO will conflict with USB-Serial-JTAG.",
            value,
        )

    return value


def esp32_s3_validate_supports(value: dict[str, Any]) -> dict[str, Any]:
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]

    if num < 0 or num > 48:
        raise cv.Invalid(f"Invalid pin number: {num} (must be 0-48)")
    if is_input:
        # All ESP32 pins support input mode
        pass

    check_strapping_pin(value, _ESP32S3_STRAPPING_PINS, _LOGGER)
    return value


def esp32_s3_final_validate_pins(full_config: ConfigType) -> None:
    """Warn about GPIO33-37 usage, but only when octal PSRAM (which uses them) is set.

    These pins are only taken by the PSRAM interface in octal mode (ESP32-S3R8 /
    S3R8V); on quad-PSRAM variants -- or when the psram block is disabled, so the
    octal interface is never configured -- they are free. The per-pin validator
    can't know the PSRAM mode, so the check is deferred here, where
    PIN_SCHEMA_REGISTRY.pins_used already lists every used pin.
    """
    # Imported locally to avoid circular import issues
    from esphome.components.psram import DOMAIN as PSRAM_DOMAIN, TYPE_OCTAL

    psram_config = full_config.get(PSRAM_DOMAIN, {})
    if psram_config.get(CONF_DISABLED) or psram_config.get(CONF_MODE) != TYPE_OCTAL:
        return
    for number in sorted(
        number
        for key, _client_id, number in PIN_SCHEMA_REGISTRY.pins_used
        if key == PLATFORM_ESP32 and number in _ESP32S3R8_PSRAM_PINS
    ):
        _LOGGER.warning(
            "GPIO%d is used by the PSRAM interface in octal mode and should be avoided",
            number,
        )
