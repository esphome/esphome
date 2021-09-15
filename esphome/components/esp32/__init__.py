from contextlib import suppress
from pathlib import Path

from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_TYPE,
    CONF_VARIANT,
    CONF_VERSION,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE
import esphome.config_validation as cv
import esphome.codegen as cg

from .const import KEY_BOARD, KEY_ESP32, KEY_VARIANT, VARIANT_ESP32C3, VARIANTS

# force import gpio to register pin schema
from .gpio import esp32_pin_to_code  # noqa


def set_core_data(config):
    CORE.data[KEY_ESP32] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = "esp32"
    conf = config[CONF_FRAMEWORK]
    if conf[CONF_TYPE] == FRAMEWORK_ESP_IDF:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "esp-idf"
    elif conf[CONF_TYPE] == FRAMEWORK_ARDUINO:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION_HINT]
    )
    CORE.data[KEY_ESP32][KEY_BOARD] = config[CONF_BOARD]
    CORE.data[KEY_ESP32][KEY_VARIANT] = config[CONF_VARIANT]
    return config


def get_esp32_variant():
    return CORE.data[KEY_ESP32][KEY_VARIANT]


def is_esp32c3():
    return get_esp32_variant() == VARIANT_ESP32C3


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


FRAMEWORK_ESP_IDF = "esp-idf"
FRAMEWORK_ARDUINO = "arduino"
FRAMEWORK_SCHEMA = cv.typed_schema(
    {
        FRAMEWORK_ESP_IDF: ESP_IDF_FRAMEWORK_SCHEMA,
        FRAMEWORK_ARDUINO: ARDUINO_FRAMEWORK_SCHEMA,
    },
    lower=True,
    space="-",
    default_type=FRAMEWORK_ARDUINO,
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
    if conf[CONF_TYPE] == FRAMEWORK_ESP_IDF:
        cg.add_platformio_option("framework", "espidf")
        cg.add_build_flag("-DUSE_ESP_IDF")
        cg.add_build_flag("-DUSE_ESP32_FRAMEWORK_ESP_IDF")
        cg.add_build_flag("-Wno-nonnull-compare")
        if conf[CONF_VERSION]:
            cg.add_platformio_option(
                "platform_packages",
                [f"platformio/framework-espidf @ {conf[CONF_VERSION]}"],
            )
    elif conf[CONF_TYPE] == FRAMEWORK_ARDUINO:
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
