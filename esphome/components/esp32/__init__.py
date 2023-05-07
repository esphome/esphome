from dataclasses import dataclass
from typing import Union
from pathlib import Path
import logging
import os

from esphome.helpers import copy_file_if_changed, write_file_if_changed, mkdir_p
from esphome.const import (
    CONF_BOARD,
    CONF_COMPONENTS,
    CONF_FRAMEWORK,
    CONF_NAME,
    CONF_SOURCE,
    CONF_TYPE,
    CONF_VARIANT,
    CONF_VERSION,
    CONF_ADVANCED,
    CONF_REFRESH,
    CONF_PATH,
    CONF_URL,
    CONF_REF,
    CONF_IGNORE_EFUSE_MAC_CRC,
    KEY_CORE,
    KEY_FRAMEWORK_VERSION,
    KEY_TARGET_FRAMEWORK,
    KEY_TARGET_PLATFORM,
    TYPE_GIT,
    TYPE_LOCAL,
    __version__,
)
from esphome.core import CORE, HexInt, TimePeriod
import esphome.config_validation as cv
import esphome.codegen as cg
from esphome import git

from .const import (  # noqa
    KEY_BOARD,
    KEY_COMPONENTS,
    KEY_ESP32,
    KEY_PATH,
    KEY_REF,
    KEY_REFRESH,
    KEY_REPO,
    KEY_SDKCONFIG_OPTIONS,
    KEY_VARIANT,
    VARIANT_ESP32C3,
    VARIANT_FRIENDLY,
    VARIANTS,
)
from .boards import BOARDS

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
        CORE.data[KEY_ESP32][KEY_COMPONENTS] = {}
    elif conf[CONF_TYPE] == FRAMEWORK_ARDUINO:
        CORE.data[KEY_CORE][KEY_TARGET_FRAMEWORK] = "arduino"
    CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION] = cv.Version.parse(
        config[CONF_FRAMEWORK][CONF_VERSION]
    )
    CORE.data[KEY_ESP32][KEY_BOARD] = config[CONF_BOARD]
    CORE.data[KEY_ESP32][KEY_VARIANT] = config[CONF_VARIANT]
    return config


def get_esp32_variant(core_obj=None):
    return (core_obj or CORE).data[KEY_ESP32][KEY_VARIANT]


def only_on_variant(*, supported=None, unsupported=None):
    """Config validator for features only available on some ESP32 variants."""
    if supported is not None and not isinstance(supported, list):
        supported = [supported]
    if unsupported is not None and not isinstance(unsupported, list):
        unsupported = [unsupported]

    def validator_(obj):
        variant = get_esp32_variant()
        if supported is not None and variant not in supported:
            raise cv.Invalid(
                f"This feature is only available on {', '.join(supported)}"
            )
        if unsupported is not None and variant in unsupported:
            raise cv.Invalid(
                f"This feature is not available on {', '.join(unsupported)}"
            )
        return obj

    return validator_


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


def add_idf_component(
    name: str, repo: str, ref: str = None, path: str = None, refresh: TimePeriod = None
):
    """Add an esp-idf component to the project."""
    if not CORE.using_esp_idf:
        raise ValueError("Not an esp-idf project")
    if name not in CORE.data[KEY_ESP32][KEY_COMPONENTS]:
        CORE.data[KEY_ESP32][KEY_COMPONENTS][name] = {
            KEY_REPO: repo,
            KEY_REF: ref,
            KEY_PATH: path,
            KEY_REFRESH: refresh,
        }


def _format_framework_arduino_version(ver: cv.Version) -> str:
    # format the given arduino (https://github.com/espressif/arduino-esp32/releases) version to
    # a PIO platformio/framework-arduinoespressif32 value
    # List of package versions: https://api.registry.platformio.org/v3/packages/platformio/tool/framework-arduinoespressif32
    if ver <= cv.Version(1, 0, 3):
        return f"~2.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"
    return f"~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"


def _format_framework_espidf_version(ver: cv.Version) -> str:
    # format the given arduino (https://github.com/espressif/esp-idf/releases) version to
    # a PIO platformio/framework-espidf value
    # List of package versions: https://api.registry.platformio.org/v3/packages/platformio/tool/framework-espidf
    return f"~3.{ver.major}{ver.minor:02d}{ver.patch:02d}.0"


# NOTE: Keep this in mind when updating the recommended version:
#  * New framework historically have had some regressions, especially for WiFi.
#    The new version needs to be thoroughly validated before changing the
#    recommended version as otherwise a bunch of devices could be bricked
#  * For all constants below, update platformio.ini (in this repo)

# The default/recommended arduino framework version
#  - https://github.com/espressif/arduino-esp32/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/tool/framework-arduinoespressif32
RECOMMENDED_ARDUINO_FRAMEWORK_VERSION = cv.Version(2, 0, 5)
# The platformio/espressif32 version to use for arduino frameworks
#  - https://github.com/platformio/platform-espressif32/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/platform/espressif32
ARDUINO_PLATFORM_VERSION = cv.Version(5, 3, 0)

# The default/recommended esp-idf framework version
#  - https://github.com/espressif/esp-idf/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/tool/framework-espidf
RECOMMENDED_ESP_IDF_FRAMEWORK_VERSION = cv.Version(4, 4, 4)
# The platformio/espressif32 version to use for esp-idf frameworks
#  - https://github.com/platformio/platform-espressif32/releases
#  - https://api.registry.platformio.org/v3/packages/platformio/platform/espressif32
ESP_IDF_PLATFORM_VERSION = cv.Version(5, 3, 0)


def _arduino_check_versions(value):
    value = value.copy()
    lookups = {
        "dev": (cv.Version(2, 1, 0), "https://github.com/espressif/arduino-esp32.git"),
        "latest": (cv.Version(2, 0, 7), None),
        "recommended": (RECOMMENDED_ARDUINO_FRAMEWORK_VERSION, None),
    }

    if value[CONF_VERSION] in lookups:
        if CONF_SOURCE in value:
            raise cv.Invalid(
                "Framework version needs to be explicitly specified when custom source is used."
            )

        version, source = lookups[value[CONF_VERSION]]
    else:
        version = cv.Version.parse(cv.version_number(value[CONF_VERSION]))
        source = value.get(CONF_SOURCE, None)

    value[CONF_VERSION] = str(version)
    value[CONF_SOURCE] = source or _format_framework_arduino_version(version)

    value[CONF_PLATFORM_VERSION] = value.get(
        CONF_PLATFORM_VERSION, _parse_platform_version(str(ARDUINO_PLATFORM_VERSION))
    )

    if version != RECOMMENDED_ARDUINO_FRAMEWORK_VERSION:
        _LOGGER.warning(
            "The selected Arduino framework version is not the recommended one. "
            "If there are connectivity or build issues please remove the manual version."
        )

    return value


def _esp_idf_check_versions(value):
    value = value.copy()
    lookups = {
        "dev": (cv.Version(5, 1, 0), "https://github.com/espressif/esp-idf.git"),
        "latest": (cv.Version(5, 0, 1), None),
        "recommended": (RECOMMENDED_ESP_IDF_FRAMEWORK_VERSION, None),
    }

    if value[CONF_VERSION] in lookups:
        if CONF_SOURCE in value:
            raise cv.Invalid(
                "Framework version needs to be explicitly specified when custom source is used."
            )

        version, source = lookups[value[CONF_VERSION]]
    else:
        version = cv.Version.parse(cv.version_number(value[CONF_VERSION]))
        source = value.get(CONF_SOURCE, None)

    if version < cv.Version(4, 0, 0):
        raise cv.Invalid("Only ESP-IDF 4.0+ is supported.")

    value[CONF_VERSION] = str(version)
    value[CONF_SOURCE] = source or _format_framework_espidf_version(version)

    value[CONF_PLATFORM_VERSION] = value.get(
        CONF_PLATFORM_VERSION, _parse_platform_version(str(ESP_IDF_PLATFORM_VERSION))
    )

    if version != RECOMMENDED_ESP_IDF_FRAMEWORK_VERSION:
        _LOGGER.warning(
            "The selected ESP-IDF framework version is not the recommended one. "
            "If there are connectivity or build issues please remove the manual version."
        )

    return value


def _parse_platform_version(value):
    try:
        # if platform version is a valid version constraint, prefix the default package
        cv.platformio_version_constraint(value)
        return f"platformio/espressif32@{value}"
    except cv.Invalid:
        return value


def _detect_variant(value):
    if CONF_VARIANT not in value:
        board = value[CONF_BOARD]
        if board not in BOARDS:
            raise cv.Invalid(
                "This board is unknown, please set the variant manually",
                path=[CONF_BOARD],
            )

        value = value.copy()
        value[CONF_VARIANT] = BOARDS[board][KEY_VARIANT]

    return value


CONF_PLATFORM_VERSION = "platform_version"

ARDUINO_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_SOURCE): cv.string_strict,
            cv.Optional(CONF_PLATFORM_VERSION): _parse_platform_version,
        }
    ),
    _arduino_check_versions,
)

CONF_SDKCONFIG_OPTIONS = "sdkconfig_options"
ESP_IDF_FRAMEWORK_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.Optional(CONF_VERSION, default="recommended"): cv.string_strict,
            cv.Optional(CONF_SOURCE): cv.string_strict,
            cv.Optional(CONF_PLATFORM_VERSION): _parse_platform_version,
            cv.Optional(CONF_SDKCONFIG_OPTIONS, default={}): {
                cv.string_strict: cv.string_strict
            },
            cv.Optional(CONF_ADVANCED, default={}): cv.Schema(
                {
                    cv.Optional(CONF_IGNORE_EFUSE_MAC_CRC, default=False): cv.boolean,
                }
            ),
            cv.Optional(CONF_COMPONENTS, default=[]): cv.ensure_list(
                cv.Schema(
                    {
                        cv.Required(CONF_NAME): cv.string_strict,
                        cv.Required(CONF_SOURCE): cv.SOURCE_SCHEMA,
                        cv.Optional(CONF_PATH): cv.string,
                        cv.Optional(CONF_REFRESH, default="1d"): cv.All(
                            cv.string, cv.source_refresh
                        ),
                    }
                )
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
            cv.Optional(CONF_VARIANT): cv.one_of(*VARIANTS, upper=True),
            cv.Optional(CONF_FRAMEWORK, default={}): FRAMEWORK_SCHEMA,
        }
    ),
    _detect_variant,
    set_core_data,
)


async def to_code(config):
    cg.add_platformio_option("board", config[CONF_BOARD])
    cg.add_build_flag("-DUSE_ESP32")
    cg.add_define("ESPHOME_BOARD", config[CONF_BOARD])
    cg.add_build_flag(f"-DUSE_ESP32_VARIANT_{config[CONF_VARIANT]}")
    cg.add_define("ESPHOME_VARIANT", VARIANT_FRIENDLY[config[CONF_VARIANT]])

    cg.add_platformio_option("lib_ldf_mode", "off")

    framework_ver: cv.Version = CORE.data[KEY_CORE][KEY_FRAMEWORK_VERSION]

    conf = config[CONF_FRAMEWORK]
    cg.add_platformio_option("platform", conf[CONF_PLATFORM_VERSION])

    cg.add_platformio_option("extra_scripts", ["post:post_build.py"])

    if conf[CONF_TYPE] == FRAMEWORK_ESP_IDF:
        cg.add_platformio_option("framework", "espidf")
        cg.add_build_flag("-DUSE_ESP_IDF")
        cg.add_build_flag("-DUSE_ESP32_FRAMEWORK_ESP_IDF")
        cg.add_build_flag("-Wno-nonnull-compare")
        cg.add_platformio_option(
            "platform_packages",
            [f"platformio/framework-espidf@{conf[CONF_SOURCE]}"],
        )
        # platformio/toolchain-esp32ulp does not support linux_aarch64 yet and has not been updated for over 2 years
        # This is espressif's own published version which is more up to date.
        cg.add_platformio_option(
            "platform_packages", ["espressif/toolchain-esp32ulp@2.35.0-20220830"]
        )
        add_idf_sdkconfig_option("CONFIG_PARTITION_TABLE_SINGLE_APP", False)
        add_idf_sdkconfig_option("CONFIG_PARTITION_TABLE_CUSTOM", True)
        add_idf_sdkconfig_option(
            "CONFIG_PARTITION_TABLE_CUSTOM_FILENAME", "partitions.csv"
        )
        add_idf_sdkconfig_option("CONFIG_COMPILER_OPTIMIZATION_DEFAULT", False)
        add_idf_sdkconfig_option("CONFIG_COMPILER_OPTIMIZATION_SIZE", True)

        # Increase freertos tick speed from 100Hz to 1kHz so that delay() resolution is 1ms
        add_idf_sdkconfig_option("CONFIG_FREERTOS_HZ", 1000)

        # Setup watchdog
        add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT", True)
        add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT_PANIC", True)
        add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0", False)
        add_idf_sdkconfig_option("CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU1", False)

        cg.add_platformio_option("board_build.partitions", "partitions.csv")

        for name, value in conf[CONF_SDKCONFIG_OPTIONS].items():
            add_idf_sdkconfig_option(name, RawSdkconfigValue(value))

        if conf[CONF_ADVANCED][CONF_IGNORE_EFUSE_MAC_CRC]:
            cg.add_define("USE_ESP32_IGNORE_EFUSE_MAC_CRC")
            if (framework_ver.major, framework_ver.minor) >= (4, 4):
                add_idf_sdkconfig_option(
                    "CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE", False
                )
            else:
                add_idf_sdkconfig_option(
                    "CONFIG_ESP32_PHY_CALIBRATION_AND_DATA_STORAGE", False
                )

        cg.add_define(
            "USE_ESP_IDF_VERSION_CODE",
            cg.RawExpression(
                f"VERSION_CODE({framework_ver.major}, {framework_ver.minor}, {framework_ver.patch})"
            ),
        )

        for component in conf[CONF_COMPONENTS]:
            source = component[CONF_SOURCE]
            if source[CONF_TYPE] == TYPE_GIT:
                add_idf_component(
                    name=component[CONF_NAME],
                    repo=source[CONF_URL],
                    ref=source.get(CONF_REF),
                    path=component.get(CONF_PATH),
                    refresh=component[CONF_REFRESH],
                )
            elif source[CONF_TYPE] == TYPE_LOCAL:
                _LOGGER.warning("Local components are not implemented yet.")

    elif conf[CONF_TYPE] == FRAMEWORK_ARDUINO:
        cg.add_platformio_option("framework", "arduino")
        cg.add_build_flag("-DUSE_ARDUINO")
        cg.add_build_flag("-DUSE_ESP32_FRAMEWORK_ARDUINO")
        cg.add_platformio_option(
            "platform_packages",
            [f"platformio/framework-arduinoespressif32@{conf[CONF_SOURCE]}"],
        )

        cg.add_platformio_option("board_build.partitions", "partitions.csv")

        cg.add_define(
            "USE_ARDUINO_VERSION_CODE",
            cg.RawExpression(
                f"VERSION_CODE({framework_ver.major}, {framework_ver.minor}, {framework_ver.patch})"
            ),
        )


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
otadata,  data, ota,     ,        0x2000,
phy_init, data, phy,     ,        0x1000,
app0,     app,  ota_0,   ,      0x1C0000,
app1,     app,  ota_1,   ,      0x1C0000,
nvs,      data, nvs,     ,       0x6d000,
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
        # IDF build scripts look for version string to put in the build.
        # However, if the build path does not have an initialized git repo,
        # and no version.txt file exists, the CMake script fails for some setups.
        # Fix by manually pasting a version.txt file, containing the ESPHome version
        write_file_if_changed(
            CORE.relative_build_path("version.txt"),
            __version__,
        )

        import shutil

        shutil.rmtree(CORE.relative_build_path("components"), ignore_errors=True)

        if CORE.data[KEY_ESP32][KEY_COMPONENTS]:
            components: dict = CORE.data[KEY_ESP32][KEY_COMPONENTS]

            for name, component in components.items():
                repo_dir, _ = git.clone_or_update(
                    url=component[KEY_REPO],
                    ref=component[KEY_REF],
                    refresh=component[KEY_REFRESH],
                    domain="idf_components",
                )
                mkdir_p(CORE.relative_build_path("components"))
                component_dir = repo_dir
                if component[KEY_PATH] is not None:
                    component_dir = component_dir / component[KEY_PATH]

                shutil.copytree(
                    component_dir,
                    CORE.relative_build_path(f"components/{name}"),
                    dirs_exist_ok=True,
                    ignore=shutil.ignore_patterns(".git", ".github"),
                )

    dir = os.path.dirname(__file__)
    post_build_file = os.path.join(dir, "post_build.py.script")
    copy_file_if_changed(
        post_build_file,
        CORE.relative_build_path("post_build.py"),
    )
