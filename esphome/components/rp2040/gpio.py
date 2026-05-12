from esphome import pins
import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import (
    CONF_ANALOG,
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
from esphome.core import CORE

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


def _board_max_virtual_pin(board):
    """Get the max CYW43 virtual pin for this board, or None if no virtual pins."""
    return boards.BOARDS.get(board, {}).get("max_virtual_pin")


def validate_gpio_pin(value):
    value = _translate_pin(value)
    board = CORE.data[KEY_RP2040][KEY_BOARD]
    max_virtual = _board_max_virtual_pin(board)
    if max_virtual is not None and boards.CYW43_GPIO_OFFSET <= value <= max_virtual:
        return value
    max_pin = boards.BOARDS.get(board, {}).get("max_pin", boards.DEFAULT_MAX_PIN)
    if value < 0 or value > max_pin:
        raise cv.Invalid(f"Invalid pin number: {value} (max {max_pin} for this board)")
    return value


def validate_supports(value):
    board = CORE.data[KEY_RP2040][KEY_BOARD]
    if (
        _board_max_virtual_pin(board) is None
        or value[CONF_NUMBER] < boards.CYW43_GPIO_OFFSET
    ):
        return value
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]
    is_output = mode[CONF_OUTPUT]
    is_open_drain = mode[CONF_OPEN_DRAIN]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]
    if not is_output or is_input or is_open_drain or is_pullup or is_pulldown:
        raise cv.Invalid("Only output mode is supported for CYW43 virtual pins")
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
    # Only set if true to avoid bloating setup() function
    # (inverted bit in pin_flags_ bitfield is zero-initialized to false)
    if config[CONF_INVERTED]:
        cg.add(var.set_inverted(True))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
