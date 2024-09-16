import esphome.codegen as cg
import esphome.config_validation as cv
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
    CONF_ANALOG,
)
from esphome.core import CORE
from esphome import pins

from . import boards
from .const import KEY_BOARD, KEY_RP2040, rp2040_ns

RP2040GPIOPin = rp2040_ns.class_("RP2040GPIOPin", cg.InternalGPIOPin)


def _lookup_pin(value):
    board = CORE.data[KEY_RP2040][KEY_BOARD]
    board_pins = boards.RP2040_BOARD_PINS.get(board, {})

    while isinstance(board_pins, str):
        board_pins = boards.RP2040_BOARD_PINS[board_pins]

    if value in board_pins:
        return board_pins[value]
    if value in boards.RP2040_BASE_PINS:
        return boards.RP2040_BASE_PINS[value]
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


def validate_gpio_pin(value):
    value = _translate_pin(value)
    board = CORE.data[KEY_RP2040][KEY_BOARD]
    if board == "rpipicow" and value == 32:
        return value  # Special case for Pico-w LED pin
    if value < 0 or value > 29:
        raise cv.Invalid(f"RP2040: Invalid pin number: {value}")
    return value


def validate_supports(value):
    board = CORE.data[KEY_RP2040][KEY_BOARD]
    if board != "rpipicow" or value[CONF_NUMBER] != 32:
        return value
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]
    is_output = mode[CONF_OUTPUT]
    is_open_drain = mode[CONF_OPEN_DRAIN]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]
    if not is_output or is_input or is_open_drain or is_pullup or is_pulldown:
        raise cv.Invalid("Only output mode is supported for Pico-w LED pin")
    return value


RP2040_PIN_SCHEMA = cv.All(
    pins.gpio_base_schema(
        RP2040GPIOPin,
        validate_gpio_pin,
        modes=pins.GPIO_STANDARD_MODES + (CONF_ANALOG,),
    ),
    validate_supports,
)


@pins.PIN_SCHEMA_REGISTRY.register("rp2040", RP2040_PIN_SCHEMA)
async def rp2040_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
