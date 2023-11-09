import logging

import esphome.codegen as cg
import esphome.config_validation as cv
from esphome import pins
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

from .const import (
    KEY_BOARD,
    KEY_COMPONENT_DATA,
    KEY_LIBRETINY,
    LibreTinyComponent,
    libretiny_ns,
)

_LOGGER = logging.getLogger(__name__)

ArduinoInternalGPIOPin = libretiny_ns.class_(
    "ArduinoInternalGPIOPin", cg.InternalGPIOPin
)


def _is_name_deprecated(value):
    return value[0] in "DA" and value[1:].isnumeric()


def _lookup_board_pins(board):
    component: LibreTinyComponent = CORE.data[KEY_LIBRETINY][KEY_COMPONENT_DATA]
    board_pins = component.board_pins.get(board, {})
    # Resolve aliased board pins (shorthand when two boards have the same pin configuration)
    while isinstance(board_pins, str):
        board_pins = board_pins[board_pins]
    return board_pins


def _lookup_pin(value):
    board: str = CORE.data[KEY_LIBRETINY][KEY_BOARD]
    board_pins = _lookup_board_pins(board)

    # check numeric pin values
    if isinstance(value, int):
        if value in board_pins.values() or not board_pins:
            # accept if pin number present in board pins
            # if board is not found, just accept all numeric values
            return value
        raise cv.Invalid(f"Pin number '{value}' is not usable for board {board}.")

    # check textual pin names
    if isinstance(value, str):
        if not board_pins:
            # can't remap without known pin name
            raise cv.Invalid(
                f"Board {board} wasn't found. "
                f"Use 'GPIO#' (numeric value) instead of '{value}'."
            )

        if value in board_pins:
            # pin name found, remap to numeric value
            if _is_name_deprecated(value):
                number = board_pins[value]
                # find all alternative pin names (except the deprecated)
                names = (
                    k
                    for k, v in board_pins.items()
                    if v == number and not _is_name_deprecated(k)
                )
                # sort by shortest
                # favor P# or PA# names
                names = sorted(
                    names,
                    key=lambda x: len(x) - 99 if x[0] == "P" else len(x),
                )
                _LOGGER.warning(
                    "Using D# and A# pin numbering is deprecated. "
                    "Please replace '%s' with one of: %s",
                    value,
                    ", ".join(names),
                )
            return board_pins[value]

        # pin name not found and not numeric
        raise cv.Invalid(f"Cannot resolve pin name '{value}' for board {board}.")

    # unknown type of the value
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
    is_analog = mode[CONF_ANALOG]
    is_input = mode[CONF_INPUT]
    is_output = mode[CONF_OUTPUT]
    is_open_drain = mode[CONF_OPEN_DRAIN]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]

    if is_open_drain and not is_output:
        raise cv.Invalid(
            "Open-drain only works with output mode", [CONF_MODE, CONF_OPEN_DRAIN]
        )
    if is_analog and not is_input:
        raise cv.Invalid("Analog pins must be an input", [CONF_MODE, CONF_ANALOG])
    if is_analog:
        # expect analog pin numbers to be available as either ADC# or A#
        number = value[CONF_NUMBER]
        board: str = CORE.data[KEY_LIBRETINY][KEY_BOARD]
        board_pins = _lookup_board_pins(board)
        analog_pins = [
            v
            for k, v in board_pins.items()
            if k[0] == "A" and k[1:].isnumeric() or k[0:3] == "ADC"
        ]
        if number not in analog_pins:
            raise cv.Invalid(
                f"Pin '{number}' is not an analog pin", [CONF_MODE, CONF_ANALOG]
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


BASE_PIN_SCHEMA = pins.gpio_base_schema(
    ArduinoInternalGPIOPin,
    validate_gpio_pin,
    modes=pins.GPIO_STANDARD_MODES + (CONF_ANALOG,),
).add_extra(validate_gpio_usage)


async def component_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
