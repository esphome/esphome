from contextlib import suppress
from functools import reduce
import logging
from pathlib import Path

from esphome.cpp_types import GPIOFlags
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_ID,
    CONF_INPUT,
    CONF_INVERTED,
    CONF_MODE,
    CONF_NUMBER,
    CONF_OPEN_DRAIN,
    CONF_OUTPUT,
    CONF_PULLDOWN,
    CONF_PULLUP,
    CONF_TYPE,
    CONF_VARIANT,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
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


def set_core_data(config):
    CORE.data[KEY_ESP32] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = "esp32"
    conf = config[CONF_FRAMEWORK]
    if conf[CONF_TYPE] == CONF_ESP_IDF:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "esp-idf"
    elif conf[CONF_TYPE] == CONF_ARDUINO:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION_HINT]
    )
    CORE.data[KEY_ESP32][KEY_BOARD] = config[CONF_BOARD]
    return config


def _arduino_check_versions(value):
    lookups = {
        "dev": ("https://github.com/espressif/arduino-esp32.git", cv.Version(2, 0, 0)),
        "latest": ("", cv.Version(1, 0, 3)),
        "recommended": ("~3.10006.0", cv.Version(1, 0, 6)),
    }
    ver_value = value[CONF_VERSION]
    default_ver_hint = None
    if ver_value.lower() in lookups:
        default_ver_hint = str(lookups[ver_value.lower()][1])
        ver_value = lookups[ver_value.lower()][0]
    else:
        with suppress(cv.Invalid):
            ver = cv.Version.parse(cv.version_number(value))
            if ver <= cv.Version(1, 0, 3):
                ver_value = f"~2.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
            else:
                ver_value = f"~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
            default_ver_hint = str(ver)

    if CONF_VERSION_HINT not in value and default_ver_hint is None:
        raise cv.Invalid("Needs a version hint to understand the framework version")

    return {
        CONF_VERSION: ver_value,
        CONF_VERSION_HINT: value.get(CONF_VERSION_HINT, default_ver_hint),
    }


def _esp_idf_check_versions(value):
    lookups = {
        "dev": ("https://github.com/espressif/esp-idf.git", cv.Version(4, 3, 1)),
        "latest": ("", cv.Version(4, 3, 0)),
        "recommended": ("~3.40300.0", cv.Version(4, 3, 0)),
    }
    ver_value = value[CONF_VERSION]
    default_ver_hint = None
    if ver_value.lower() in lookups:
        default_ver_hint = str(lookups[ver_value.lower()][1])
        ver_value = lookups[ver_value.lower()][0]
    else:
        with suppress(cv.Invalid):
            ver = cv.Version.parse(cv.version_number(value))
            ver_value = f"~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
            default_ver_hint = str(ver)

    if CONF_VERSION_HINT not in value and default_ver_hint is None:
        raise cv.Invalid("Needs a version hint to understand the framework version")

    return {
        CONF_VERSION: ver_value,
        CONF_VERSION_HINT: value.get(CONF_VERSION_HINT, default_ver_hint),
    }


CONF_VERSION_HINT = "version_hint"
ARDUINO_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_VERSION_HINT): cv.version_number,
        }
    ),
    _arduino_check_versions,
)
ESP_IDF_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_VERSION_HINT): cv.version_number,
        }
    ),
    _esp_idf_check_versions,
)


CONF_ESP_IDF = "esp-idf"
CONF_ARDUINO = "arduino"
FRAMEWORK_SCHEMA = cv.typed_schema(
    {
        CONF_ESP_IDF: ESP_IDF_FRAMEWORK_SCHEMA,
        CONF_ARDUINO: ARDUINO_FRAMEWORK_SCHEMA,
    },
    lower=True,
    space="-",
    default_type=CONF_ARDUINO,
)


CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Required(CONF_BOARD): cv.string_strict,
            cv.Optional(CONF_VARIANT, default="ESP32"): cv.one_of(
                *VARIANTS, upper=True
            ),
            cv.Optional(CONF_FRAMEWORK, default={}): FRAMEWORK_SCHEMA,
        }
    ),
    set_core_data,
)


async def to_code(config):
    cg.add_platformio_option("platform", "espressif32")
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_ESP32")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_build_flag(f"-DUSE_ESP32_VARIANT_{config[CONF_VARIANT]}")

    conf = config[CONF_FRAMEWORK]
    if conf[CONF_TYPE] == CONF_ESP_IDF:
        cg.add_platformio_option("framework", "espidf")
        cg.add_build_flag("-DUSE_ESP_IDF")
        cg.add_build_flag("-DUSE_ESP32_FRAMEWORK_ESP_IDF")
        cg.add_build_flag("-Wno-nonnull-compare")
        if conf[CONF_VERSION]:
            cg.add_platformio_option(
                "platform_packages",
                [f"platformio/framework-espidf @ {conf[CONF_VERSION]}"],
            )
    elif conf[CONF_TYPE] == CONF_ARDUINO:
        cg.add_platformio_option("framework", "arduino")
        cg.add_build_flag("-DUSE_ARDUINO")
        cg.add_build_flag("-DUSE_ESP32_FRAMEWORK_ARDUINO")
        if conf[CONF_VERSION]:
            cg.add_platformio_option(
                "platform_packages",
                [f"platformio/framework-arduinoespressif32 @ {conf[CONF_VERSION]}"],
            )

        # Set build partitions, stored in this directory
        esp32_dir = Path(__file__).parent
        partition_csv_loc = esp32_dir / "partitions.csv"
        cg.add_platformio_option("board_build.partitions", str(partition_csv_loc))


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
