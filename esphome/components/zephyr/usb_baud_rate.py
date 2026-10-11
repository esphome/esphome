from collections.abc import Callable
from typing import Any

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.core import ID
from esphome.cpp_generator import MockObjClass

from .const import zephyr_ns

UsbBaudRate = zephyr_ns.class_("UsbBaudRate", cg.Component)
CONF_USB_BAUD_RATE = "usb_baud_rate"


def declare_component_id(component: MockObjClass) -> Callable[[Any], ID]:
    """cv.declare_id that also accepts an ID an earlier validation already declared, so a
    validated config can be validated again (the cached config is fed back through the
    schema)."""
    declare = cv.declare_id(component)

    def validator(value: Any) -> ID:
        return value if isinstance(value, ID) else declare(value)

    return validator
