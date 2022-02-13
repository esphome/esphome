from dataclasses import dataclass
from typing import Any

from esphome.const import (
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
)
from esphome import pins
from esphome.core import CORE
import esphome.config_validation as cv
import esphome.codegen as cg

from . import boards
from .const import (
    KEY_BOARD,
    KEY_ESP32,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32C3,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32H2,
    esp32_ns,
)


from .gpio_esp32 import esp32_validate_gpio_pin, esp32_validate_supports
from .gpio_esp32_s2 import esp32_s2_validate_gpio_pin, esp32_s2_validate_supports
from .gpio_esp32_c3 import esp32_c3_validate_gpio_pin, esp32_c3_validate_supports
from .gpio_esp32_s3 import esp32_s3_validate_gpio_pin, esp32_s3_validate_supports
from .gpio_esp32_h2 import esp32_h2_validate_gpio_pin, esp32_h2_validate_supports


IDFInternalGPIOPin = esp32_ns.class_("IDFInternalGPIOPin", cg.InternalGPIOPin)
ArduinoInternalGPIOPin = esp32_ns.class_("ArduinoInternalGPIOPin", cg.InternalGPIOPin)


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
    if isinstance(value, int):
        return value
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
    VARIANT_ESP32H2: ESP32ValidationFunctions(
        pin_validation=esp32_h2_validate_gpio_pin,
        usage_validation=esp32_h2_validate_supports,
    ),
}


def validate_gpio_pin(value):
    value = _translate_pin(value)
    variant = CORE.data[KEY_ESP32][KEY_VARIANT]
    if variant not in _esp32_validations:
        raise cv.Invalid("Unsupported ESP32 variant {variant}")

    return _esp32_validations[variant].pin_validation(value)


def validate_supports(value):
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]
    is_output = mode[CONF_OUTPUT]
    is_open_drain = mode[CONF_OPEN_DRAIN]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]
    variant = CORE.data[KEY_ESP32][KEY_VARIANT]
    if variant not in _esp32_validations:
        raise cv.Invalid("Unsupported ESP32 variant {variant}")

    if is_open_drain and not is_output:
        raise cv.Invalid(
            "Open-drain only works with output mode", [CONF_MODE, CONF_OPEN_DRAIN]
        )

    value = _esp32_validations[variant].usage_validation(value)
    if CORE.using_arduino:
        # (input, output, open_drain, pullup, pulldown)
        supported_modes = {
            # INPUT
            (True, False, False, False, False),
            # OUTPUT
            (False, True, False, False, False),
            # INPUT_PULLUP
            (True, False, False, True, False),
            # INPUT_PULLDOWN
            (True, False, False, False, True),
            # OUTPUT_OPEN_DRAIN
            (False, True, True, False, False),
        }
        key = (is_input, is_output, is_open_drain, is_pullup, is_pulldown)
        if key not in supported_modes:
            raise cv.Invalid(
                "This pin mode is not supported on ESP32 for arduino frameworks",
                [CONF_MODE],
            )
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


def _choose_pin_declaration(value):
    if CORE.using_esp_idf:
        return cv.declare_id(IDFInternalGPIOPin)(value)
    if CORE.using_arduino:
        return cv.declare_id(ArduinoInternalGPIOPin)(value)
    raise NotImplementedError


CONF_DRIVE_STRENGTH = "drive_strength"
ESP32_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): _choose_pin_declaration,
        cv.Required(CONF_NUMBER): validate_gpio_pin,
        cv.Optional(CONF_MODE, default={}): cv.Schema(
            {
                cv.Optional(CONF_INPUT, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT, default=False): cv.boolean,
                cv.Optional(CONF_OPEN_DRAIN, default=False): cv.boolean,
                cv.Optional(CONF_PULLUP, default=False): cv.boolean,
                cv.Optional(CONF_PULLDOWN, default=False): cv.boolean,
            }
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
        cv.SplitDefault(CONF_DRIVE_STRENGTH, esp32_idf="20mA"): cv.All(
            cv.only_with_esp_idf,
            cv.float_with_unit("current", "mA", optional_unit=True),
            cv.enum(DRIVE_STRENGTHS),
        ),
    },
    validate_supports,
)


@pins.PIN_SCHEMA_REGISTRY.register("esp32", ESP32_PIN_SCHEMA)
async def esp32_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    num = config[CONF_NUMBER]
    if CORE.using_esp_idf:
        cg.add(var.set_pin(getattr(gpio_num_t, f"GPIO_NUM_{num}")))
    else:
        cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    if CONF_DRIVE_STRENGTH in config:
        cg.add(var.set_drive_strength(config[CONF_DRIVE_STRENGTH]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
