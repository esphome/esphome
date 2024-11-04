from dataclasses import dataclass
import logging
from typing import Any

from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ID,
    CONF_IGNORE_PIN_VALIDATION_ERROR,
    CONF_IGNORE_STRAPPING_WARNING,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    PLATFORM_ESP32,
)
from esphome.core import CORE

from . import boards
from .const import (
    KEY_BOARD,
    KEY_ESP32,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32C2,
    VARIANT_ESP32C3,
    VARIANT_ESP32C6,
    VARIANT_ESP32H2,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    esp32_ns,
)
from .gpio_esp32 import esp32_validate_gpio_pin, esp32_validate_supports
from .gpio_esp32_c2 import esp32_c2_validate_gpio_pin, esp32_c2_validate_supports
from .gpio_esp32_c3 import esp32_c3_validate_gpio_pin, esp32_c3_validate_supports
from .gpio_esp32_c6 import esp32_c6_validate_gpio_pin, esp32_c6_validate_supports
from .gpio_esp32_h2 import esp32_h2_validate_gpio_pin, esp32_h2_validate_supports
from .gpio_esp32_s2 import esp32_s2_validate_gpio_pin, esp32_s2_validate_supports
from .gpio_esp32_s3 import esp32_s3_validate_gpio_pin, esp32_s3_validate_supports

ESP32InternalGPIOPin = esp32_ns.class_("ESP32InternalGPIOPin", cg.InternalGPIOPin)


_LOGGER = logging.getLogger(__name__)


def _lookup_pin(value):
    board = CORE.data[KEY_ESP32][KEY_BOARD]
    board_pins = boards.ESP32_BOARD_PINS.get(board, {})

    # Resolved aliased board pins (shorthand when two boards have the same pin configuration)
    while isinstance(board_pins, str):
        board_pins = boards.ESP32_BOARD_PINS[board_pins]

    if value in board_pins:
        return board_pins[value]
    if value in boards.ESP32_BASE_PINS:
        return boards.ESP32_BASE_PINS[value]
    raise cv.Invalid(f"Cannot resolve pin name '{value}' for board {board}.")


def _translate_pin(value):
    if isinstance(value, dict) or value is None:
        raise cv.Invalid(
            "This variable only supports pin numbers, not full pin schemas "
            "(with inverted and mode)."
        )
    if isinstance(value, int) and not isinstance(value, bool):
        return value
    if not isinstance(value, str):
        raise cv.Invalid(f"Invalid pin number: {value}")
    try:
        return int(value)
    except ValueError:
        pass
    if value.startswith("GPIO"):
        return cv.int_(value[len("GPIO") :].strip())
    return _lookup_pin(value)


@dataclass
class ESP32ValidationFunctions:
    pin_validation: Any
    usage_validation: Any


_esp32_validations = {
    VARIANT_ESP32: ESP32ValidationFunctions(
        pin_validation=esp32_validate_gpio_pin, usage_validation=esp32_validate_supports
    ),
    VARIANT_ESP32S2: ESP32ValidationFunctions(
        pin_validation=esp32_s2_validate_gpio_pin,
        usage_validation=esp32_s2_validate_supports,
    ),
    VARIANT_ESP32C3: ESP32ValidationFunctions(
        pin_validation=esp32_c3_validate_gpio_pin,
        usage_validation=esp32_c3_validate_supports,
    ),
    VARIANT_ESP32S3: ESP32ValidationFunctions(
        pin_validation=esp32_s3_validate_gpio_pin,
        usage_validation=esp32_s3_validate_supports,
    ),
    VARIANT_ESP32C2: ESP32ValidationFunctions(
        pin_validation=esp32_c2_validate_gpio_pin,
        usage_validation=esp32_c2_validate_supports,
    ),
    VARIANT_ESP32C6: ESP32ValidationFunctions(
        pin_validation=esp32_c6_validate_gpio_pin,
        usage_validation=esp32_c6_validate_supports,
    ),
    VARIANT_ESP32H2: ESP32ValidationFunctions(
        pin_validation=esp32_h2_validate_gpio_pin,
        usage_validation=esp32_h2_validate_supports,
    ),
}


def gpio_pin_number_validator(value):
    value = _translate_pin(value)
    board = CORE.data[KEY_ESP32][KEY_BOARD]
    board_pins = boards.ESP32_BOARD_PINS.get(board, {})

    # Resolved aliased board pins (shorthand when two boards have the same pin configuration)
    while isinstance(board_pins, str):
        board_pins = boards.ESP32_BOARD_PINS[board_pins]

    if value in board_pins.values():
        return value

    variant = CORE.data[KEY_ESP32][KEY_VARIANT]
    if variant not in _esp32_validations:
        raise cv.Invalid(f"Unsupported ESP32 variant {variant}")

    return value


def validate_gpio_pin(pin):
    variant = CORE.data[KEY_ESP32][KEY_VARIANT]
    if variant not in _esp32_validations:
        raise cv.Invalid(f"Unsupported ESP32 variant {variant}")

    ignore_pin_validation_warning = pin[CONF_IGNORE_PIN_VALIDATION_ERROR]
    try:
        pin[CONF_NUMBER] = _esp32_validations[variant].pin_validation(pin[CONF_NUMBER])
    except cv.Invalid as exc:
        if not ignore_pin_validation_warning:
            raise

        _LOGGER.warning(
            "Ignoring validation error on pin %d; error: %s",
            pin[CONF_NUMBER],
            exc,
        )
    else:
        # Throw an exception if used for a pin that would not have resulted
        # in a validation error anyway!
        if ignore_pin_validation_warning:
            raise cv.Invalid(f"GPIO{pin[CONF_NUMBER]} is not a reserved pin")

    return pin


def validate_supports(value):
    mode = value[CONF_MODE]
    is_output = mode[CONF_OUTPUT]
    is_open_drain = mode[CONF_OPEN_DRAIN]
    variant = CORE.data[KEY_ESP32][KEY_VARIANT]
    if variant not in _esp32_validations:
        raise cv.Invalid(f"Unsupported ESP32 variant {variant}")

    if is_open_drain and not is_output:
        raise cv.Invalid(
            "Open-drain only works with output mode", [CONF_MODE, CONF_OPEN_DRAIN]
        )

    value = _esp32_validations[variant].usage_validation(value)
    return value


# https://docs.espressif.com/projects/esp-idf/en/v3.3.5/api-reference/peripherals/gpio.html#_CPPv416gpio_drive_cap_t
gpio_drive_cap_t = cg.global_ns.enum("gpio_drive_cap_t")
DRIVE_STRENGTHS = {
    5.0: gpio_drive_cap_t.GPIO_DRIVE_CAP_0,
    10.0: gpio_drive_cap_t.GPIO_DRIVE_CAP_1,
    20.0: gpio_drive_cap_t.GPIO_DRIVE_CAP_2,
    40.0: gpio_drive_cap_t.GPIO_DRIVE_CAP_3,
}
gpio_num_t = cg.global_ns.enum("gpio_num_t")

CONF_DRIVE_STRENGTH = "drive_strength"

ESP32_PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(ESP32InternalGPIOPin, gpio_pin_number_validator).extend(
        {
            cv.Optional(CONF_IGNORE_PIN_VALIDATION_ERROR, default=False): cv.boolean,
            cv.Optional(CONF_IGNORE_STRAPPING_WARNING, default=False): cv.boolean,
            cv.Optional(CONF_DRIVE_STRENGTH, default="20mA"): cv.All(
                cv.float_with_unit("current", "mA", optional_unit=True),
                cv.enum(DRIVE_STRENGTHS),
            ),
        }
    ),
    validate_gpio_pin,
    validate_supports,
)


@pins.PIN_SCHEMA_REGISTRY.register(PLATFORM_ESP32, ESP32_PIN_SCHEMA)
async def esp32_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(getattr(gpio_num_t, f"GPIO_NUM_{num}")))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    if CONF_DRIVE_STRENGTH in config:
        cg.add(var.set_drive_strength(config[CONF_DRIVE_STRENGTH]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
