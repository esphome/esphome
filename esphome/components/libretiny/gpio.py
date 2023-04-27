import re

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
import esphome.config_validation as cv
import esphome.codegen as cg

from .const import libretiny_ns

ArduinoInternalGPIOPin = libretiny_ns.class_(
    "ArduinoInternalGPIOPin", cg.InternalGPIOPin
)


def _translate_pin(value):
    if isinstance(value, dict) or value is None:
        raise cv.Invalid(
            "This variable only supports pin numbers, not full pin schemas "
            "(with inverted and mode)."
        )
    try:
        value = int(value)
    except ValueError:
        pass
    if isinstance(value, int):
        raise cv.Invalid(
            "Using raw pin numbers is ambiguous. Use either "
            f"P{value}/GPIO{value} or D{value} - these are "
            "different pins! Refer to the docs for details."
        )
    # accept only strings at this point
    if not isinstance(value, str):
        raise cv.Invalid(f"Pin number {value} is of an unknown type.")
    # macros shouldn't be changed
    if value.startswith("PIN_"):
        return value
    # make GPIO* equal to P*
    if value.startswith("GPIO"):
        value = "P" + value[4:]
    # check the final name using a regex
    if not re.match(r"^[A-Z]+[0-9]+$", value):
        raise cv.Invalid("Invalid pin name")
    # translate the name to a macro
    return f"PIN_{value}"


def validate_gpio_pin(value):
    return _translate_pin(value)


def validate_mode(value):
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
    return value


LIBRETINY_PIN_SCHEMA = cv.All(
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
    validate_mode,
)


@pins.PIN_SCHEMA_REGISTRY.register("libretiny", LIBRETINY_PIN_SCHEMA)
async def libretiny_pin_to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    num = config[CONF_NUMBER]
    if not str(num).isnumeric():
        num = cg.global_ns.namespace(num)
    cg.add(var.set_pin(num))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_flags(pins.gpio_flags_expr(config[CONF_MODE])))
    return var
