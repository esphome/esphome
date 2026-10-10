"""Pin checks shared by the ESP32 variant validators."""

from logging import Logger
from typing import Any

from esphome.components.const import CONF_HOLD_STATE
from esphome.const import CONF_NUMBER


def check_usb_jtag_pin(num: int, usb_jtag_pins: set[int], logger: Logger) -> None:
    if num in usb_jtag_pins:
        logger.warning(
            "GPIO%d is used by the USB-Serial-JTAG interface."
            " Using this pin as GPIO will conflict with USB-Serial-JTAG.",
            num,
        )


def check_usb_jtag_hold(
    conf: dict[str, Any], usb_jtag_pins: set[int], logger: Logger
) -> None:
    num = conf[CONF_NUMBER]
    if conf.get(CONF_HOLD_STATE) and num in usb_jtag_pins:
        logger.warning(
            "GPIO%d cannot hold at low level during wakeup from deep sleep.", num
        )
