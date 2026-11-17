from collections.abc import Callable
import logging

from esphome import pins
import esphome.codegen as cg
from esphome.components.const import CONF_DRIVE_STRENGTH, CONF_HOLD_STATE
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
from esphome.types import ConfigType

from . import boards
from .const import (
    KEY_BOARD,
    KEY_ESP32,
    KEY_VARIANT,
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
    esp32_ns,
)
from .gpio_common import ESP32PinSpec
from .gpio_esp32 import ESP32_PIN_SPEC
from .gpio_esp32_c2 import ESP32_C2_PIN_SPEC
from .gpio_esp32_c3 import ESP32_C3_PIN_SPEC
from .gpio_esp32_c5 import ESP32_C5_PIN_SPEC
from .gpio_esp32_c6 import ESP32_C6_PIN_SPEC
from .gpio_esp32_c61 import ESP32_C61_PIN_SPEC
from .gpio_esp32_h2 import ESP32_H2_PIN_SPEC
from .gpio_esp32_h4 import ESP32_H4_PIN_SPEC
from .gpio_esp32_h21 import ESP32_H21_PIN_SPEC
from .gpio_esp32_p4 import ESP32_P4_PIN_SPEC
from .gpio_esp32_s2 import ESP32_S2_PIN_SPEC
from .gpio_esp32_s3 import ESP32_S3_PIN_SPEC, esp32_s3_final_validate_pins
from .gpio_esp32_s31 import ESP32_S31_PIN_SPEC

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
            "This variable only supports pin numbers, not full pin schemas (with inverted and mode)."
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


_esp32_pin_specs: dict[str, ESP32PinSpec] = {
    VARIANT_ESP32: ESP32_PIN_SPEC,
    VARIANT_ESP32C2: ESP32_C2_PIN_SPEC,
    VARIANT_ESP32C3: ESP32_C3_PIN_SPEC,
    VARIANT_ESP32C5: ESP32_C5_PIN_SPEC,
    VARIANT_ESP32C6: ESP32_C6_PIN_SPEC,
    VARIANT_ESP32C61: ESP32_C61_PIN_SPEC,
    VARIANT_ESP32H2: ESP32_H2_PIN_SPEC,
    VARIANT_ESP32H4: ESP32_H4_PIN_SPEC,
    VARIANT_ESP32H21: ESP32_H21_PIN_SPEC,
    VARIANT_ESP32P4: ESP32_P4_PIN_SPEC,
    VARIANT_ESP32S2: ESP32_S2_PIN_SPEC,
    VARIANT_ESP32S3: ESP32_S3_PIN_SPEC,
    VARIANT_ESP32S31: ESP32_S31_PIN_SPEC,
}
_final_validations: dict[str, Callable[[ConfigType], None]] = {
    VARIANT_ESP32S3: esp32_s3_final_validate_pins,
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
    if variant not in _esp32_pin_specs:
        raise cv.Invalid(f"Unsupported ESP32 variant {variant}")

    return value


def validate_gpio_pin(pin):
    variant = CORE.data[KEY_ESP32][KEY_VARIANT]
    if variant not in _esp32_pin_specs:
        raise cv.Invalid(f"Unsupported ESP32 variant {variant}")

    ignore_pin_validation_warning = pin[CONF_IGNORE_PIN_VALIDATION_ERROR]
    try:
        pin[CONF_NUMBER] = _esp32_pin_specs[variant].validate_gpio_pin(pin[CONF_NUMBER])
    except cv.Invalid as exc:
        if not ignore_pin_validation_warning:
            raise

        _LOGGER.warning(
            "Ignoring validation error on pin %d; error: %s",
            pin[CONF_NUMBER],
            exc,
        )
    else:
        # `ignore_pin_validation_error` only suppresses an error raised by
        # validate_gpio_pin above (e.g. SPI flash/PSRAM pins); pin numbers out of
        # range still fail validate_supports. If nothing raised, the option is a
        # no-op -- warn so the user can clean it up, but don't block the build.
        if ignore_pin_validation_warning:
            _LOGGER.warning(
                "GPIO%d has no validation errors to ignore; "
                "remove `ignore_pin_validation_error: true` from this pin.",
                pin[CONF_NUMBER],
            )

    return pin


def validate_supports(value):
    mode = value[CONF_MODE]
    is_output = mode[CONF_OUTPUT]
    is_open_drain = mode[CONF_OPEN_DRAIN]
    variant = CORE.data[KEY_ESP32][KEY_VARIANT]
    if variant not in _esp32_pin_specs:
        raise cv.Invalid(f"Unsupported ESP32 variant {variant}")

    if is_open_drain and not is_output:
        raise cv.Invalid(
            "Open-drain only works with output mode", [CONF_MODE, CONF_OPEN_DRAIN]
        )

    return _esp32_pin_specs[variant].validate_supports(value)


# https://docs.espressif.com/projects/esp-idf/en/v3.3.5/api-reference/peripherals/gpio.html#_CPPv416gpio_drive_cap_t
gpio_drive_cap_t = cg.global_ns.enum("gpio_drive_cap_t")
DRIVE_STRENGTHS = {
    5.0: gpio_drive_cap_t.GPIO_DRIVE_CAP_0,
    10.0: gpio_drive_cap_t.GPIO_DRIVE_CAP_1,
    20.0: gpio_drive_cap_t.GPIO_DRIVE_CAP_2,
    40.0: gpio_drive_cap_t.GPIO_DRIVE_CAP_3,
}
gpio_num_t = cg.global_ns.enum("gpio_num_t")


ESP32_PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(ESP32InternalGPIOPin, gpio_pin_number_validator).extend(
        {
            cv.Optional(CONF_IGNORE_PIN_VALIDATION_ERROR, default=False): cv.boolean,
            cv.Optional(CONF_IGNORE_STRAPPING_WARNING, default=False): cv.boolean,
            cv.Optional(CONF_DRIVE_STRENGTH, default="20mA"): cv.All(
                cv.float_with_unit("current", "mA", optional_unit=True),
                cv.enum(DRIVE_STRENGTHS),
            ),
            cv.Optional(CONF_HOLD_STATE, default=False): cv.boolean,
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
    # Only set if true to avoid bloating setup() function
    # (inverted bit in pin_flags_ bitfield is zero-initialized to false)
    if config[CONF_INVERTED]:
        cg.add(var.set_inverted(True))
    cg.add_optional(var.set_drive_strength, config.get(CONF_DRIVE_STRENGTH))
    flags = pins.gpio_flags_expr(config[CONF_MODE])
    if config[CONF_HOLD_STATE]:
        flags = flags | cg.gpio_Flags.FLAG_HOLD
        cg.add_define("USE_GPIO_HOLD")
    cg.add(var.set_flags(flags))
    return var


def final_validate_pins(full_config: ConfigType) -> None:
    """Run the active variant's pin final-validation, if it defines one."""
    if (final := _final_validations.get(CORE.data[KEY_ESP32][KEY_VARIANT])) is not None:
        final(full_config)
