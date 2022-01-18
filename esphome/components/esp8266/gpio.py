import logging

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
from .const import KEY_BOARD, KEY_ESP8266, esp8266_ns


_LOGGER = logging.getLogger(__name__)


ESP8266GPIOPin = esp8266_ns.class_("ESP8266GPIOPin", cg.InternalGPIOPin)


def _lookup_pin(value):
    board = CORE.data[KEY_ESP8266][KEY_BOARD]
    board_pins = boards.ESP8266_BOARD_PINS.get(board, {})

    # Resolved aliased board pins (shorthand when two boards have the same pin configuration)
    while isinstance(board_pins, str):
        board_pins = boards.ESP8266_BOARD_PINS[board_pins]

    if value in board_pins:
        return board_pins[value]
    if value in boards.ESP8266_BASE_PINS:
        return boards.ESP8266_BASE_PINS[value]
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


_ESP_SDIO_PINS = {
    6: "Flash Clock",
    7: "Flash Data 0",
    8: "Flash Data 1",
    11: "Flash Command",
}


def validate_gpio_pin(value):
    value = _translate_pin(value)
    if value < 0 or value > 17:
        raise cv.Invalid(f"ESP8266: Invalid pin number: {value}")
    if value in _ESP_SDIO_PINS:
        raise cv.Invalid(
            f"This pin cannot be used on ESP8266s and is already used by the flash interface (function: {_ESP_SDIO_PINS[value]})"
        )
    if 9 <= value <= 10:
        _LOGGER.warning(
            "ESP8266: Pin %s (9-10) might already be used by the "
            "flash interface in QUAD IO flash mode.",
            value,
        )
    return value


def validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    is_input = mode[CONF_INPUT]
    is_output = mode[CONF_OUTPUT]
    is_open_drain = mode[CONF_OPEN_DRAIN]
    is_pullup = mode[CONF_PULLUP]
    is_pulldown = mode[CONF_PULLDOWN]
    is_analog = mode[CONF_ANALOG]

    if (not is_analog) and num == 17:
        raise cv.Invalid(
            "GPIO17 (TOUT) is an analog-only pin on the ESP8266.",
            [CONF_MODE],
        )
    if is_analog and num != 17:
        raise cv.Invalid(
            "Only GPIO17 is analog-capable on ESP8266.",
            [CONF_MODE, CONF_ANALOG],
        )
    if is_open_drain and not is_output:
        raise cv.Invalid(
            "Open-drain only works with output mode", [CONF_MODE, CONF_OPEN_DRAIN]
        )
    if is_pullup and num == 16:
        raise cv.Invalid(
            "GPIO Pin 16 does not support pullup pin mode. "
            "Please choose another pin.",
            [CONF_MODE, CONF_PULLUP],
        )
    if is_pulldown and num != 16:
        raise cv.Invalid("Only GPIO16 supports pulldown.", [CONF_MODE, CONF_PULLDOWN])

    # (input, output, open_drain, pullup, pulldown)
    supported_modes = {
        # INPUT
        (True, False, False, False, False),
        # OUTPUT
        (False, True, False, False, False),
        # INPUT_PULLUP
        (True, False, False, True, False),
        # INPUT_PULLDOWN_16
        (True, False, False, False, True),
        # OUTPUT_OPEN_DRAIN
        (False, True, True, False, False),
    }
    key = (is_input, is_output, is_open_drain, is_pullup, is_pulldown)
    if key not in supported_modes:
        raise cv.Invalid(
            "This pin mode is not supported on ESP8266",
            [CONF_MODE],
        )

    return value


CONF_ANALOG = "analog"
ESP8266_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(ESP8266GPIOPin),
        cv.Required(CONF_NUMBER): validate_gpio_pin,
        cv.Optional(CONF_MODE, default={}): cv.Schema(
            {
                cv.Optional(CONF_ANALOG, default=False): cv.boolean,
                cv.Optional(CONF_INPUT, default=False): cv.boolean,
                cv.Optional(CONF_OUTPUT, default=False): cv.boolean,
                cv.Optional(CONF_OPEN_DRAIN, default=False): cv.boolean,
                cv.Optional(CONF_PULLUP, default=False): cv.boolean,
                cv.Optional(CONF_PULLDOWN, default=False): cv.boolean,
            }
        ),
        cv.Optional(CONF_INVERTED, default=False): cv.boolean,
    },
    validate_supports,
)


@pins.PIN_SCHEMA_REGISTRY.register("esp8266", ESP8266_PIN_SCHEMA)
async def esp8266_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    num = config[CONF_NUMBER]
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
