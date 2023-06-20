import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
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
from esphome.core import CORE

from .const import (
    KEY_BOARD,
    KEY_COMPONENT_DATA,
    KEY_LIBRETINY,
    LibreTinyComponent,
    libretiny_ns,
)

ArduinoInternalGPIOPin = libretiny_ns.class_(
    "ArduinoInternalGPIOPin", cg.InternalGPIOPin
)


def _lookup_pin(value):
    board: str = CORE.data[KEY_LIBRETINY][KEY_BOARD]
    component: LibreTinyComponent = CORE.data[KEY_LIBRETINY][KEY_COMPONENT_DATA]
    board_pins = component.board_pins.get(board, {})

    # Resolve aliased board pins (shorthand when two boards have the same pin configuration)
    while isinstance(board_pins, str):
        board_pins = board_pins[board_pins]

    if isinstance(value, int):
        if value in board_pins.values() or not board_pins:
            # if board is not found, just accept numeric values
            return value
        raise cv.Invalid(f"Pin number '{value}' is not usable for board {board}.")
    if isinstance(value, str):
        if value in board_pins:
            return board_pins[value]
        if not board_pins:
            raise cv.Invalid(
                f"Board {board} wasn't found. "
                f"Use 'GPIO#' (numeric value) instead of '{value}'."
            )
        raise cv.Invalid(f"Cannot resolve pin name '{value}' for board {board}.")
    raise cv.Invalid(f"Unrecognized pin value '{value}'.")


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
    # translate GPIO* and P* to a number, if possible
    # otherwise return unchanged value (i.e. pin PA05)
    try:
        if value.startswith("GPIO"):
            value = int(value[4:])
        elif value.startswith("P"):
            value = int(value[1:])
    except ValueError:
        pass
    return value


def validate_gpio_pin(value):
    value = _translate_pin(value)
    value = _lookup_pin(value)

    component: LibreTinyComponent = CORE.data[KEY_LIBRETINY][KEY_COMPONENT_DATA]
    if component.pin_validation:
        value = component.pin_validation(value)

    return value


def validate_gpio_usage(value):
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]
    is_output = mode[CONF_OUTPUT]
    is_open_drain = mode[CONF_OPEN_DRAIN]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]

    if is_open_drain and not is_output:
        raise cv.Invalid(
            "Open-drain only works with output mode", [CONF_MODE, CONF_OPEN_DRAIN]
        )

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
        raise cv.Invalid("This pin mode is not supported", [CONF_MODE])

    component: LibreTinyComponent = CORE.data[KEY_LIBRETINY][KEY_COMPONENT_DATA]
    if component.usage_validation:
        value = component.usage_validation(value)

    return value


BASE_PIN_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(ArduinoInternalGPIOPin),
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
    },
)

BASE_PIN_SCHEMA.add_extra(validate_gpio_usage)


async def component_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
