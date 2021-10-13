from dataclasses import dataclass
from typing import Union
from pathlib import Path
import logging

from esphome.helpers import write_file_if_changed
from esphome.const import (
    CONF_BOARD,
    CONF_FRAMEWORK,
    CONF_TYPE,
    CONF_VARIANT,
    CONF_VERSION,
    CONF_ADVANCED,
    CONF_IGNORE_EFUSE_MAC_CRC,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
)
from esphome.core import CORE, HexInt
import esphome.config_validation as cv
import esphome.codegen as cg

from .const import (  # noqa
    KEY_BOARD,
    KEY_ESP32,
    KEY_SDKCONFIG_OPTIONS,
    KEY_VARIANT,
    VARIANT_ESP32,
    VARIANT_ESP32S2,
    VARIANT_ESP32S3,
    VARIANT_ESP32C3,
    VARIANT_ESP32H2,
    VARIANTS,
)

# force import gpio to register pin schema
from .gpio import esp32_pin_to_code  # noqa


_LOGGER = logging.getLogger(__name__)
CODEOWNERS = ["@esphome/core"]
AUTO_LOAD = ["preferences"]


def set_core_data(config):
    CORE.data[KEY_ESP32] = {}
    CORE.data[KEY_CORE][KEY_TARGET_PLATFORM] = "esp32"
    conf = config[CONF_FRAMEWORK]
    if conf[CONF_TYPE] == FRAMEWORK_ESP_IDF:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "esp-idf"
        CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS] = {}
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


@dataclass
class RawSdkconfigValue:
    """An sdkconfig value that won't be auto-formatted"""

    value: str


SdkconfigValueType = Union[bool, int, HexInt, str, RawSdkconfigValue]


def add_idf_sdkconfig_option(name: str, value: SdkconfigValueType):
    """Set an esp-idf sdkconfig value."""
    if not CORE.using_esp_idf:
        raise ValueError("Not an esp-idf project")
    CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS][name] = value


def _format_framework_arduino_version(ver: cv.Version) -> str:
    # format the given arduino (https://github.com/espressif/arduino-esp32/releases) version to
    # a PIO platformio/framework-arduinoespressif32 value
    # List of package versions: https://api.registry.platformio.org/v3/packages/platformio/tool/framework-arduinoespressif32
    if ver <= cv.Version(1, 0, 3):
        return f"~2.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
    return f"~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"


# NOTE: Keep this in mind when updating the recommended version:
#  * New framework historically have had some regressions, especially for WiFi.
#    The new version needs to be thoroughly validated before changing the
#    recommended version as otherwise a bunch of devices could be bricked
#  * For all constants below, update platformio.ini (in this repo)
#    and platformio.ini/platformio-lint.ini in the esphome-docker-base repository

# The default/recommended arduino framework version
#  - https://github.com/espressif/arduino-esp32/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/tool/framework-arduinoespressif32
RECOMMENDED_ARDUINO_FRAMEWORK_VERSION = cv.Version(1, 0, 6)
# The platformio/espressif32 version to use for arduino frameworks
#  - https://github.com/platformio/platform-espressif32/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/platform/espressif32
ARDUINO_PLATFORM_VERSION = cv.Version(3, 3, 2)

# The default/recommended esp-idf framework version
#  - https://github.com/espressif/esp-idf/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/tool/framework-espidf
RECOMMENDED_ESP_IDF_FRAMEWORK_VERSION = cv.Version(4, 3, 0)
# The platformio/espressif32 version to use for esp-idf frameworks
#  - https://github.com/platformio/platform-espressif32/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/platform/espressif32
ESP_IDF_PLATFORM_VERSION = cv.Version(3, 3, 2)


def _arduino_check_versions(value):
    value = value.copy()
    lookups = {
        "dev": ("https://github.com/espressif/arduino-esp32.git", cv.Version(2, 0, 0)),
        "latest": ("", cv.Version(1, 0, 3)),
        "recommended": (
            _format_framework_arduino_version(RECOMMENDED_ARDUINO_FRAMEWORK_VERSION),
            RECOMMENDED_ARDUINO_FRAMEWORK_VERSION,
        ),
    }
    ver_value = value[CONF_VERSION]
    default_ver_hint = None
    if ver_value.lower() in lookups:
        default_ver_hint = str(lookups[ver_value.lower()][1])
        ver_value = lookups[ver_value.lower()][0]
    else:
        with cv.suppress_invalid():
            ver = cv.Version.parse(cv.version_number(value))
            if ver <= cv.Version(1, 0, 3):
                ver_value = f"~2.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
            else:
                ver_value = f"~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
            default_ver_hint = str(ver)
    value[CONF_VERSION] = ver_value

    if CONF_VERSION_HINT not in value and default_ver_hint is None:
        raise cv.Invalid("Needs a version hint to understand the framework version")

    ver_hint_s = value.get(CONF_VERSION_HINT, default_ver_hint)
    value[CONF_VERSION_HINT] = ver_hint_s
    plat_ver = value.get(CONF_PLATFORM_VERSION, ARDUINO_PLATFORM_VERSION)
    value[CONF_PLATFORM_VERSION] = str(plat_ver)

    if cv.Version.parse(ver_hint_s) != RECOMMENDED_ARDUINO_FRAMEWORK_VERSION:
        _LOGGER.warning(
            "The selected arduino framework version is not the recommended one"
        )
        _LOGGER.warning(
            "If there are connectivity or build issues please remove the manual version"
        )

    return value


def _format_framework_espidf_version(ver: cv.Version) -> str:
    # format the given arduino (https://github.com/espressif/esp-idf/releases) version to
    # a PIO platformio/framework-espidf value
    # List of package versions: https://api.registry.platformio.org/v3/packages/platformio/tool/framework-espidf
    return f"~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"


def _esp_idf_check_versions(value):
    value = value.copy()
    lookups = {
        "dev": ("https://github.com/espressif/esp-idf.git", cv.Version(4, 3, 1)),
        "latest": ("", cv.Version(4, 3, 0)),
        "recommended": (
            _format_framework_espidf_version(RECOMMENDED_ESP_IDF_FRAMEWORK_VERSION),
            RECOMMENDED_ESP_IDF_FRAMEWORK_VERSION,
        ),
    }
    ver_value = value[CONF_VERSION]
    default_ver_hint = None
    if ver_value.lower() in lookups:
        default_ver_hint = str(lookups[ver_value.lower()][1])
        ver_value = lookups[ver_value.lower()][0]
    else:
        with cv.suppress_invalid():
            ver = cv.Version.parse(cv.version_number(value))
            ver_value = f"~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
            default_ver_hint = str(ver)
    value[CONF_VERSION] = ver_value

    if CONF_VERSION_HINT not in value and default_ver_hint is None:
        raise cv.Invalid("Needs a version hint to understand the framework version")

    ver_hint_s = value.get(CONF_VERSION_HINT, default_ver_hint)
    value[CONF_VERSION_HINT] = ver_hint_s
    if cv.Version.parse(ver_hint_s) < cv.Version(4, 0, 0):
        raise cv.Invalid("Only ESP-IDF 4.0+ is supported")
    if cv.Version.parse(ver_hint_s) != RECOMMENDED_ESP_IDF_FRAMEWORK_VERSION:
        _LOGGER.warning(
            "The selected esp-idf framework version is not the recommended one"
        )
        _LOGGER.warning(
            "If there are connectivity or build issues please remove the manual version"
        )

    plat_ver = value.get(CONF_PLATFORM_VERSION, ESP_IDF_PLATFORM_VERSION)
    value[CONF_PLATFORM_VERSION] = str(plat_ver)

    return value


CONF_VERSION_HINT = "version_hint"
CONF_PLATFORM_VERSION = "platform_version"
ARDUINO_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_VERSION_HINT): cv.version_number,
            cv.Optional(CONF_PLATFORM_VERSION): cv.string_strict,
        }
    ),
    _arduino_check_versions,
)
CONF_SDKCONFIG_OPTIONS = "sdkconfig_options"
ESP_IDF_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_VERSION_HINT): cv.version_number,
            cv.Optional(CONF_SDKCONFIG_OPTIONS, default={}): {
                cv.string_strict: cv.string_strict
            },
            cv.Optional(CONF_PLATFORM_VERSION): cv.string_strict,
            cv.Optional(CONF_ADVANCED, default={}): cv.Schema(
                {
                    cv.Optional(CONF_IGNORE_EFUSE_MAC_CRC, default=False): cv.boolean,
                }
            ),
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
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_ESP32")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_build_flag(f"-DUSE_ESP32_VARIANT_{config[CONF_VARIANT]}")

    cg.add_platformio_option("lib_ldf_mode", "off")

    conf = config[CONF_FRAMEWORK]
    if conf[CONF_TYPE] == FRAMEWORK_ESP_IDF:
        cg.add_platformio_option(
            "platform", f"espressif32 @ {conf[CONF_PLATFORM_VERSION]}"
        )
        cg.add_platformio_option("framework", "espidf")
        cg.add_build_flag("-DUSE_ESP_IDF")
        cg.add_build_flag("-DUSE_ESP32_FRAMEWORK_ESP_IDF")
        cg.add_build_flag("-Wno-nonnull-compare")
        cg.add_platformio_option(
            "platform_packages",
            [f"platformio/framework-espidf @ {conf[CONF_VERSION]}"],
        )
        add_idf_sdkconfig_option("CONFIG_PARTITION_TABLE_SINGLE_APP", False)
        add_idf_sdkconfig_option("CONFIG_PARTITION_TABLE_CUSTOM", True)
        add_idf_sdkconfig_option(
            "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME", "partitions.csv"
        )
        add_idf_sdkconfig_option("CONFIG_COMPILER_OPTIMIZATION_DEFAULT", False)
        add_idf_sdkconfig_option("CONFIG_COMPILER_OPTIMIZATION_SIZE", True)

        cg.add_platformio_option("board_build.partitions", "partitions.csv")

        for name, value in conf[CONF_SDKCONFIG_OPTIONS].items():
            add_idf_sdkconfig_option(name, RawSdkconfigValue(value))

        if conf[CONF_ADVANCED][CONF_IGNORE_EFUSE_MAC_CRC]:
            cg.add_define("USE_ESP32_IGNORE_EFUSE_MAC_CRC")
            add_idf_sdkconfig_option(
                "CONFIG_ESP32_PHY_CALIBRATION_AND_DATA_STORAGE", False
            )

    elif conf[CONF_TYPE] == FRAMEWORK_ARDUINO:
        cg.add_platformio_option(
            "platform", f"espressif32 @ {conf[CONF_PLATFORM_VERSION]}"
        )
        cg.add_platformio_option("framework", "arduino")
        cg.add_build_flag("-DUSE_ARDUINO")
        cg.add_build_flag("-DUSE_ESP32_FRAMEWORK_ARDUINO")
        cg.add_platformio_option(
            "platform_packages",
            [f"platformio/framework-arduinoespressif32 @ {conf[CONF_VERSION]}"],
        )

        cg.add_platformio_option("board_build.partitions", "partitions.csv")


ARDUINO_PARTITIONS_CSV = """\
nvs,      data, nvs,     0x009000, 0x005000,
otadata,  data, ota,     0x00e000, 0x002000,
app0,     app,  ota_0,   0x010000, 0x1C0000,
app1,     app,  ota_1,   0x1D0000, 0x1C0000,
eeprom,   data, 0x99,    0x390000, 0x001000,
spiffs,   data, spiffs,  0x391000, 0x00F000
"""


IDF_PARTITIONS_CSV = """\
# Name,   Type, SubType, Offset,   Size, Flags
nvs,      data, nvs,     ,        0x4000,
otadata,  data, ota,     ,        0x2000,
phy_init, data, phy,     ,        0x1000,
app0,     app,  ota_0,   ,      0x1C0000,
app1,     app,  ota_1,   ,      0x1C0000,
"""


def _format_sdkconfig_val(value: SdkconfigValueType) -> str:
    if isinstance(value, bool):
        return "y" if value else "n"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, str):
        return f'"{value}"'
    if isinstance(value, RawSdkconfigValue):
        return value.value
    raise ValueError


def _write_sdkconfig():
    # sdkconfig.{name} stores the real sdkconfig (modified by esp-idf with default)
    # sdkconfig.{name}.esphomeinternal stores what esphome last wrote
    # we use the internal one to detect if there were any changes, and if so write them to the
    # real sdkconfig
    sdk_path = Path(CORE.relative_build_path(f"sdkconfig.{CORE.name}"))
    internal_path = Path(
        CORE.relative_build_path(f"sdkconfig.{CORE.name}.esphomeinternal")
    )

    want_opts = CORE.data[KEY_ESP32][KEY_SDKCONFIG_OPTIONS]
    contents = (
        "\n".join(
            f"{name}={_format_sdkconfig_val(value)}"
            for name, value in sorted(want_opts.items())
        )
        + "\n"
    )
    if write_file_if_changed(internal_path, contents):
        # internal changed, update real one
        write_file_if_changed(sdk_path, contents)


# Called by writer.py
def copy_files():
    if CORE.using_arduino:
        write_file_if_changed(
            CORE.relative_build_path("partitions.csv"),
            ARDUINO_PARTITIONS_CSV,
        )
    if CORE.using_esp_idf:
        _write_sdkconfig()
        write_file_if_changed(
            CORE.relative_build_path("partitions.csv"),
            IDF_PARTITIONS_CSV,
        )
