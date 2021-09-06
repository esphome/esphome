from functools import reduce
import logging

from esphome.cpp_types import GPIOFlags
from esphome.const import (
    CONF_BOARD,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
    CONF_VARIANT,
    KEY_CORE,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from esphome import pins
from esphome.core import CORE
import esphome.config_validation as cv
import esphome.codegen as cg

from . import boards

_LOGGER = logging.getLogger(__name__)

esp32_ns = cg.esphome_ns.namespace("esp32")
ESP32InternalGPIOPin = esp32_ns.class_("ESP32InternalGPIOPin", cg.InternalGPIOPin)
KEY_ESP32 = "esp32"
KEY_BOARD = "board"

VARIANTS = ["ESP32", "ESP32S2", "ESP32S3", "ESP32C3", "ESP32H2"]

# Lookup table from ESP32 arduino framework version to latest platformio
# package with that version
# See also https://github.com/platformio/platform-espressif32/releases
ARDUINO_VERSION_ESP32 = {
    "dev": "https://github.com/platformio/platform-espressif32.git",
    "1.0.6": "platformio/espressif32@3.2.0",
    "1.0.5": "platformio/espressif32@3.1.1",
    "1.0.4": "platformio/espressif32@3.0.0",
    "1.0.3": "platformio/espressif32@1.10.0",
    "1.0.2": "platformio/espressif32@1.9.0",
    "1.0.1": "platformio/espressif32@1.7.0",
    "1.0.0": "platformio/espressif32@1.5.0",
}


def set_core_data(config):
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = "esp32"
    CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "esp-idf"
    CORE.data[KEY_ESP32] = {}
    CORE.data[KEY_ESP32][KEY_BOARD] = config[CONF_BOARD]
    return config


CONFIG_SCHEMA = cv.All(
    {
        cv.Optional(CONF_BOARD, default="nodemcu-32s"): cv.string_strict,
        cv.Optional(CONF_VARIANT, default="ESP32"): cv.one_of(*VARIANTS, upper=True),
    },
    set_core_data,
)


async def to_code(config):
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_platformio_option("framework", "espidf")
    cg.add_platformio_option("platform", "espressif32")
    cg.add_build_flag("-DUSE_ESP32")
    if config[CONF_VARIANT] != "ESP32":
        cg.add_build_flag(f"-DUSE_{config[CONF_VARIANT]}")
    cg.add_build_flag("-DUSE_ESP_IDF")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])


def _lookup_pin(value):
    # TODO: lookup from esp32 schema
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


_ESP_SDIO_PINS = {
    6: "Flash Clock",
    7: "Flash Data 0",
    8: "Flash Data 1",
    11: "Flash Command",
}


def validate_gpio_pin(value):
    value = _translate_pin(value)
    if value < 0 or value > 39:
        raise cv.Invalid(f"Invalid pin number: {value} (must be 0-39)")
    if value in _ESP_SDIO_PINS:
        raise cv.Invalid(
            "This pin cannot be used on ESP32s and is already used by "
            "the flash interface (function: {})".format(_ESP_SDIO_PINS[value])
        )
    if 9 <= value <= 10:
        _LOGGER.warning(
            "Pin %s (9-10) might already be used by the "
            "flash interface in QUAD IO flash mode.",
            value,
        )
    if value in (20, 24, 28, 29, 30, 31):
        # These pins are not exposed in GPIO mux (reason unknown)
        # but they're missing from IO_MUX list in datasheet
        raise cv.Invalid(f"The pin GPIO{value} is not usable on ESP32s.")
    return value


def validate_supports(value):
    num = value[CONF_NUMBER]
    mode = value[CONF_MODE]
    if mode[CONF_INPUT]:
        # All ESP32 pins support input mode
        pass
    if mode[CONF_OUTPUT] and 34 <= num <= 39:
        raise cv.Invalid(
            f"GPIO{num} (34-39) does not support output pin mode.",
            [CONF_MODE, CONF_OUTPUT],
        )
    if mode[CONF_OPEN_DRAIN] and not mode[CONF_OUTPUT]:
        raise cv.Invalid(
            "Open-drain only works with output mode", [CONF_MODE, CONF_OPEN_DRAIN]
        )
    if mode[CONF_PULLUP] and 34 <= num <= 39:
        raise cv.Invalid(
            f"GPIO{num} (34-39) does not support pullups.", [CONF_MODE, CONF_PULLUP]
        )
    if mode[CONF_PULLDOWN] and 34 <= num <= 39:
        raise cv.Invalid(
            f"GPIO{num} (34-39) does not support pulldowns.", [CONF_MODE, CONF_PULLDOWN]
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


CONF_DRIVE_STRENGTH = "drive_strength"
ESP32_PIN_SCHEMA = cv.All(
    {
        cv.GenerateID(): cv.declare_id(ESP32InternalGPIOPin),
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
        cv.Optional(CONF_DRIVE_STRENGTH, default="20mA"): cv.All(
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
    cg.add(var.set_pin(getattr(gpio_num_t, f"GPIO_NUM_{num}")))
    cg.add(var.set_inverted(config[CONF_INVERTED]))
    cg.add(var.set_drive_strength(config[CONF_DRIVE_STRENGTH]))
    flags = {
        CONF_INPUT: GPIOFlags.INPUT,
        CONF_OUTPUT: GPIOFlags.OUTPUT,
        CONF_OPEN_DRAIN: GPIOFlags.OPEN_DRAIN,
        CONF_PULLUP: GPIOFlags.PULLUP,
        CONF_PULLDOWN: GPIOFlags.PULLDOWN,
    }
    flags2 = [v for k, v in flags.items() if config[CONF_MODE][k]]
    if flags2:
        import operator

        flags3 = reduce(operator.or_, flags2)
    else:
        flags3 = GPIOFlags.NONE
    cg.add(var.set_flags(flags3))
    return var
