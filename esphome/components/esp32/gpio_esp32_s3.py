import logging
from typing import Any

import esphome.config_validation as cv
from esphome.const import CONF_INPUT, CONF_MODE, CONF_NUMBER
from esphome.core import CORE
from esphome.pins import check_strapping_pin

# CORE.data key recording any GPIO33-37 usage. Kept outside CORE.data[KEY_ESP32]
# (which set_core_data resets) so the esp32 final_validate can decide whether to
# warn: these pins are only taken by the PSRAM interface in octal mode, which is
# not known yet when an individual pin is validated.
KEY_ESP32S3R8_PSRAM_PINS_USED = "esp32_s3_r8_psram_pins_used"

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
    if value in _ESP32S3R8_PSRAM_PINS:
        # Record usage; the esp32 final_validate warns only if octal PSRAM (which
        # actually uses these pins) is configured. On quad-PSRAM S3 variants these
        # pins are free, so warning unconditionally here is a false positive.
        CORE.data.setdefault(KEY_ESP32S3R8_PSRAM_PINS_USED, set()).add(value)

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
